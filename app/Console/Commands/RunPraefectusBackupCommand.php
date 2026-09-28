<?php

declare(strict_types=1);

namespace App\Console\Commands;

use App\Models\BackupProfile;
use App\Models\BackupRun;
use App\Services\BackupProfilePreview;
use Illuminate\Console\Command;
use Illuminate\Contracts\Process\ProcessResult;
use Illuminate\Process\Exceptions\ProcessTimedOutException;
use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\File;
use Illuminate\Support\Facades\Process;
use Illuminate\Support\Sleep;
use RecursiveCallbackFilterIterator;
use RecursiveDirectoryIterator;
use RecursiveIteratorIterator;
use RuntimeException;
use SplFileInfo;
use Symfony\Component\Console\Helper\ProgressIndicator;
use Throwable;

class RunPraefectusBackupCommand extends Command
{
    protected $signature = 'praefectus:run';

    protected $description = 'Run the configured Praefectus backup';

    public function handle(BackupProfilePreview $preview): int
    {
        $profile = BackupProfile::first();

        if (! $profile instanceof BackupProfile) {
            $this->components->error(__('No backup profile configured.'));

            return self::FAILURE;
        }

        $destination = $this->destination();

        $lock = Cache::lock('praefectus:run', (int) config('praefectus.lock_seconds'));

        if (! $lock->get()) {
            BackupRun::create([
                'backup_profile_id' => $profile->id,
                'status' => 'skipped',
                'destination' => $destination,
                'file_count' => 0,
                'archive_size_bytes' => 0,
                'error_summary' => 'Backup already running.',
                'started_at' => now(),
                'finished_at' => now(),
            ]);

            $this->components->warn(__('Backup already running.'));

            return self::SUCCESS;
        }

        $run = BackupRun::create([
            'backup_profile_id' => $profile->id,
            'status' => 'running',
            'destination' => $destination,
            'file_count' => 0,
            'archive_size_bytes' => 0,
            'started_at' => now(),
        ]);

        try {
            $this->components->info(__('Validating backup profile...'));
            $this->validateProfile($profile);
            $this->components->info(__('[1/4] Scanning source directories...'));
            $selectedFiles = $preview->includedFiles($profile);
            $this->components->info(__('Selected :count files for backup.', ['count' => count($selectedFiles)]));
            $workingDirectory = rtrim((string) config('praefectus.local_backup_path'), '/');
            $timestamp = now()->format('Ymd-His');

            if ($selectedFiles === []) {
                $run->update([
                    'status' => 'skipped',
                    'finished_at' => now(),
                    'error_summary' => 'No files matched backup profile rules.',
                ]);

                $this->components->warn(__('No files matched backup profile rules.'));

                return self::SUCCESS;
            }

            if (! is_dir($workingDirectory)) {
                mkdir($workingDirectory, 0777, true);
            }

            $manifestPath = $workingDirectory.'/manifest-'.$timestamp.'.txt';
            $archivePath = $workingDirectory.'/backup-'.$timestamp.'.tar.gz';

            file_put_contents($manifestPath, implode(PHP_EOL, $selectedFiles));

            $this->components->info(__('[2/4] Creating compressed archive...'));
            $tarResult = $this->runWithActivity(
                ['tar', '--ignore-failed-read', '-czf', $archivePath, '-T', $manifestPath],
                __('Creating archive'),
            );

            if ($tarResult->failed()) {
                $run->update([
                    'status' => 'failed',
                    'finished_at' => now(),
                    'error_summary' => trim($tarResult->errorOutput()) ?: __('Backup archive creation failed.'),
                ]);

                $this->components->error(__('Backup archive creation failed.'));

                return self::FAILURE;
            }

            $this->components->info(__('Archive created.'));
            $run->update([
                'file_count' => count($selectedFiles),
                'archive_size_bytes' => file_exists($archivePath) ? (int) filesize($archivePath) : 0,
            ]);
            $this->components->info(__('[3/4] Preparing Proton Drive folders...'));
            $this->ensureRemoteDirectory($this->remoteDirectory(), $this->computerName());
            $this->ensureRemoteDirectory($this->remoteDirectory().'/'.$this->computerName(), now()->format('Y-m-d'));

            $this->components->info(__('[4/4] Uploading archive to Proton Drive...'));
            $uploadResult = $this->uploadArchive($archivePath, $destination);

            if ($uploadResult->failed()) {
                $run->update([
                    'status' => 'failed',
                    'finished_at' => now(),
                    'file_count' => count($selectedFiles),
                    'archive_size_bytes' => file_exists($archivePath) ? (int) filesize($archivePath) : 0,
                    'error_summary' => trim($uploadResult->errorOutput()) ?: __('Backup upload failed.'),
                ]);

                $this->components->error(__('Backup upload failed.'));

                return self::FAILURE;
            }

            $this->components->info(__('Removing local backup files...'));

            $localBackupFiles = array_merge(
                glob($workingDirectory.'/backup-*.tar.gz') ?: [],
                glob($workingDirectory.'/manifest-*.txt') ?: [],
            );

            foreach (array_unique([...$localBackupFiles, $archivePath, $manifestPath]) as $path) {
                if (is_file($path) && ! File::delete($path)) {
                    $this->components->warn(__('Backup uploaded, but could not delete local file: :path', ['path' => $path]));
                }
            }

            if ($this->isEmptyDirectory($workingDirectory) && ! File::deleteDirectory($workingDirectory)) {
                $this->components->warn(__('Backup uploaded, but could not delete temporary directory: :path', ['path' => $workingDirectory]));
            }

            $run->update([
                'status' => 'succeeded',
                'finished_at' => now(),
                'file_count' => count($selectedFiles),
                'archive_size_bytes' => $run->archive_size_bytes,
            ]);

            $this->components->info(__('Backup completed successfully.'));

            return self::SUCCESS;
        } catch (Throwable $exception) {
            $errorSummary = trim($exception->getMessage()) ?: __('Unexpected backup failure.');

            $run->update([
                'status' => 'failed',
                'finished_at' => now(),
                'error_summary' => $errorSummary,
            ]);

            $this->components->error($errorSummary);

            return self::FAILURE;
        } finally {
            $lock->release();
        }
    }

    private function destination(): string
    {
        return $this->remoteDirectory().'/'.$this->computerName().'/'.now()->format('Y-m-d');
    }

    private function remoteDirectory(): string
    {
        return rtrim((string) config('praefectus.remote_root'), '/');
    }

    private function computerName(): string
    {
        return (string) config('praefectus.computer_name');
    }

    private function ensureRemoteDirectory(string $parentPath, string $directoryName): void
    {
        $path = rtrim($parentPath, '/').'/'.$directoryName;
        $infoResult = $this->runWithActivity(
            [
                (string) config('praefectus.proton_bin'),
                'filesystem',
                'info',
                $path,
            ],
            __('Checking remote folder'),
        );

        if ($infoResult->successful()) {
            return;
        }

        $createResult = $this->runWithActivity(
            [
                (string) config('praefectus.proton_bin'),
                'filesystem',
                'create-folder',
                $parentPath,
                $directoryName,
            ],
            __('Creating remote folder'),
        );

        if ($createResult->failed()) {
            throw new RuntimeException(
                trim($createResult->errorOutput()) ?: __('Unable to create the Proton Drive backup folder.'),
            );
        }
    }

    private function uploadArchive(string $archivePath, string $destination): ProcessResult
    {
        $attempts = max(1, (int) config('praefectus.upload_attempts'));
        $lastResult = null;

        for ($attempt = 1; $attempt <= $attempts; $attempt++) {
            $this->components->info(__('Upload attempt :attempt of :attempts...', [
                'attempt' => $attempt,
                'attempts' => $attempts,
            ]));

            try {
                $lastResult = $this->runWithActivity(
                    [
                        'nice',
                        '-n',
                        '10',
                        (string) config('praefectus.proton_bin'),
                        'filesystem',
                        'upload',
                        $archivePath,
                        $destination,
                    ],
                    __('Uploading archive'),
                );
            } catch (ProcessTimedOutException $exception) {
                if ($attempt === $attempts) {
                    throw $exception;
                }

                $this->components->warn(__('Upload attempt timed out after :seconds seconds. Retrying...', [
                    'seconds' => $exception->exceededTimeout(),
                ]));

                continue;
            }

            if ($lastResult->successful()) {
                return $lastResult;
            }

            if ($attempt < $attempts) {
                $this->components->warn(__('Upload attempt failed. Retrying...'));
            }
        }

        return $lastResult;
    }

    /** @param array<int, string> $command */
    private function runWithActivity(array $command, string $label): ProcessResult
    {
        $startedAt = now();
        $lastReportedSeconds = 0;
        $result = null;
        $indicator = $this->output->isDecorated()
            ? new ProgressIndicator($this->output, 'verbose')
            : null;

        $indicator?->start($label);

        try {
            $process = Process::timeout((int) config('praefectus.process_timeout'))->start($command);

            while ($process->running()) {
                $process->ensureNotTimedOut();
                $indicator?->advance();

                $elapsedSeconds = (int) $startedAt->diffInSeconds(now());

                if ($indicator === null && $elapsedSeconds - $lastReportedSeconds >= 10) {
                    $this->components->info(__(':stage: still running (:elapsed elapsed).', [
                        'stage' => $label,
                        'elapsed' => $elapsedSeconds.'s',
                    ]));
                    $lastReportedSeconds = $elapsedSeconds;
                }

                Sleep::usleep(100000);
            }

            return $result = $process->wait();
        } finally {
            $indicator?->finish(
                $label.': '.($result?->successful() ? __('completed') : __('failed')),
                $result?->successful() ? '✔' : '!',
            );
        }
    }

    /** @return array<int, string> */
    private function selectedFiles(BackupProfile $profile): array
    {
        $files = [];
        $excludedDirectoryNames = $this->stringList($profile->excluded_directory_names);
        $excludedFileGlobs = $this->stringList($profile->excluded_file_globs);
        $sourceDirectories = $this->stringList($profile->source_directories);
        $sourceFiles = $this->stringList($profile->source_files);

        foreach ($sourceFiles as $sourceFile) {
            if (is_file($sourceFile) && $this->isIncludedFile($sourceFile, $excludedFileGlobs)) {
                $files[] = $sourceFile;
            }
        }

        foreach ($sourceDirectories as $sourceDirectory) {
            if (! is_dir($sourceDirectory)) {
                continue;
            }

            $iterator = new RecursiveIteratorIterator(
                new RecursiveCallbackFilterIterator(
                    new RecursiveDirectoryIterator($sourceDirectory, RecursiveDirectoryIterator::SKIP_DOTS),
                    function (SplFileInfo $file) use ($excludedDirectoryNames, $excludedFileGlobs, $sourceDirectory): bool {
                        if ($file->isDir()) {
                            return ! in_array($file->getFilename(), $excludedDirectoryNames, true);
                        }

                        return $file->isFile()
                            && $this->isIncludedFile($file->getPathname(), $excludedFileGlobs, $sourceDirectory);
                    },
                ),
            );

            foreach ($iterator as $file) {
                if ($file instanceof SplFileInfo && $file->isFile()) {
                    $files[] = $file->getPathname();
                }
            }
        }

        $files = array_values(array_unique($files));
        sort($files);

        return $files;
    }

    /** @param array<int, string> $excludedFileGlobs */
    private function isIncludedFile(string $path, array $excludedFileGlobs, ?string $sourceDirectory = null): bool
    {
        $normalizedPath = str_replace(DIRECTORY_SEPARATOR, '/', $path);
        $relativePath = $sourceDirectory === null
            ? null
            : ltrim(str_replace(DIRECTORY_SEPARATOR, '/', substr($path, strlen($sourceDirectory))), '/');

        foreach ($excludedFileGlobs as $excludedFileGlob) {
            $normalizedGlob = str_replace(DIRECTORY_SEPARATOR, '/', trim($excludedFileGlob));
            $normalizedGlob = preg_replace('#^\./#', '', $normalizedGlob) ?? $normalizedGlob;
            $candidates = array_filter([
                basename($normalizedPath),
                $normalizedPath,
                $relativePath,
            ]);

            foreach ($candidates as $candidate) {
                if (fnmatch($normalizedGlob, $candidate)
                    || str_starts_with($candidate, rtrim($normalizedGlob, '/').'/')) {
                    return false;
                }
            }
        }

        return true;
    }

    private function isEmptyDirectory(string $directory): bool
    {
        return is_dir($directory)
            && File::files($directory) === []
            && File::directories($directory) === [];
    }

    private function validateProfile(BackupProfile $profile): void
    {
        $sourceDirectories = $this->stringList($profile->source_directories);

        $sourceFiles = $this->stringList($profile->source_files);

        if ($sourceDirectories === [] && $sourceFiles === []) {
            throw new RuntimeException(__('No source directories or files configured.'));
        }

        foreach ($sourceFiles as $sourceFile) {
            if (! str_starts_with($sourceFile, DIRECTORY_SEPARATOR)) {
                throw new RuntimeException(__('Source files must use absolute paths.'));
            }

            if (! is_file($sourceFile)) {
                throw new RuntimeException(__('Source file does not exist.'));
            }
        }

        foreach ($sourceDirectories as $sourceDirectory) {
            if (! str_starts_with($sourceDirectory, DIRECTORY_SEPARATOR)) {
                throw new RuntimeException(__('Source directories must use absolute paths.'));
            }

            if (! is_dir($sourceDirectory)) {
                throw new RuntimeException(__('Source directory does not exist.'));
            }
        }
    }

    /** @return array<int, string> */
    private function stringList(mixed $value): array
    {
        if (! is_array($value)) {
            return [];
        }

        return array_values(array_filter($value, function (mixed $item): bool {
            return is_string($item) && $item !== '';
        }));
    }
}
