<?php

declare(strict_types=1);

namespace Tests\Feature\Commands;

use App\Models\BackupProfile;
use App\Models\BackupRun;
use Illuminate\Contracts\Process\ProcessResult;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Illuminate\Process\Exceptions\ProcessTimedOutException;
use Illuminate\Process\InvokedProcess;
use Illuminate\Process\PendingProcess;
use Illuminate\Support\Facades\Artisan;
use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Process;
use Illuminate\Support\Sleep;
use Mockery;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\Attributes\TestWith;
use RuntimeException;
use Symfony\Component\Console\Output\BufferedOutput;
use Symfony\Component\Console\Output\OutputInterface;
use Symfony\Component\Process\Exception\ProcessTimedOutException as SymfonyProcessTimedOutException;
use Symfony\Component\Process\Process as SymfonyProcess;
use Tests\TestCase;

class RunPraefectusBackupCommandTest extends TestCase
{
    use RefreshDatabase;

    private string $sourceDirectory;

    protected function setUp(): void
    {
        parent::setUp();

        $this->sourceDirectory = '/tmp/opencode/praefectus-command-'.uniqid();

        mkdir($this->sourceDirectory.'/vendor', 0777, true);
        mkdir($this->sourceDirectory.'/nested', 0777, true);

        file_put_contents($this->sourceDirectory.'/keep.txt', 'keep');
        file_put_contents($this->sourceDirectory.'/nested/notes.md', 'keep too');
        file_put_contents($this->sourceDirectory.'/vendor/skip.txt', 'skip');
        file_put_contents($this->sourceDirectory.'/archive.zip', 'skip');
    }

    protected function tearDown(): void
    {
        $this->deleteDirectory($this->sourceDirectory);

        $this->travelTo(null);

        parent::tearDown();
    }

    #[Test]
    public function it_runs_a_profile_driven_backup_and_records_a_successful_run(): void
    {
        $this->travelTo('2026-09-04 13:45:00');
        mkdir($this->sourceDirectory.'/.config/omarchy', 0777, true);
        file_put_contents($this->sourceDirectory.'/.config/omarchy/shell.json', 'skip');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip', '.config/omarchy'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::result(),
        ]);

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('Validating backup profile...')
            ->expectsOutputToContain('[1/4] Scanning source directories...')
            ->expectsOutputToContain('Selected 2 files for backup.')
            ->expectsOutputToContain('[2/4] Creating compressed archive...')
            ->expectsOutputToContain('Archive created.')
            ->expectsOutputToContain('[3/4] Preparing Proton Drive folders...')
            ->expectsOutputToContain('[4/4] Uploading archive to Proton Drive...')
            ->expectsOutputToContain('Upload attempt 1 of 2...')
            ->doesntExpectOutputToContain('Retrying...')
            ->expectsOutputToContain('Backup completed successfully.')
            ->assertSuccessful();

        $run = BackupRun::first();

        $this->assertSame('succeeded', $run->status);

        $this->assertSame(2, $run->file_count);

        $this->assertSame('/my-files/backups/'.gethostname().'/2026-09-04', $run->destination);

        $this->assertNotNull($run->started_at);

        $this->assertNotNull($run->finished_at);

        Process::assertRan(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'tar'
                && in_array('--ignore-failed-read', $process->command, true)
                && in_array('-T', $process->command, true);
        });

        Process::assertRan(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'nice'
                && $process->command[1] === '-n'
                && $process->command[2] === '10'
                && $process->command[3] === 'proton-drive'
                && $process->command[4] === 'filesystem'
                && $process->command[5] === 'upload';
        });
    }

    #[Test]
    public function it_includes_configured_source_files_in_the_backup(): void
    {
        $sourceFile = $this->sourceDirectory.'/keep.txt';

        BackupProfile::factory()->create([
            'source_directories' => [],
            'source_files' => [$sourceFile],
        ]);

        Process::preventStrayProcesses();
        Process::fake(function (PendingProcess $process) use ($sourceFile): ProcessResult {
            if ($process->command[0] === 'tar') {
                $manifestPath = $process->command[array_search('-T', $process->command, true) + 1];

                $this->assertStringContainsString($sourceFile, file_get_contents($manifestPath));
            }

            return Process::result();
        });

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('Selected 1 files for backup.')
            ->assertSuccessful();

        $this->assertSame(1, BackupRun::first()->file_count);
    }

    #[Test]
    public function it_creates_missing_remote_directories_before_uploading(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::sequence()
                ->push(Process::result())
                ->push(Process::result(errorOutput: 'Node not found', exitCode: 1))
                ->push(Process::result())
                ->push(Process::result(errorOutput: 'Node not found', exitCode: 1))
                ->push(Process::result())
                ->push(Process::result()),
        ]);

        $this->artisan('praefectus:run')->assertSuccessful();

        Process::assertRanTimes(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'proton-drive'
                && $process->command[1] === 'filesystem'
                && $process->command[2] === 'create-folder';
        }, 2);
    }

    #[Test]
    public function it_retries_the_upload_once_before_marking_the_run_successful(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::sequence()
                ->push(Process::result())
                ->push(Process::result())
                ->push(Process::result())
                ->push(Process::result(errorOutput: 'temporary upload error', exitCode: 1))
                ->push(Process::result()),
        ]);

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('Upload attempt 1 of 2...')
            ->expectsOutputToContain('Upload attempt failed. Retrying...')
            ->expectsOutputToContain('Upload attempt 2 of 2...')
            ->expectsOutputToContain('Backup completed successfully.')
            ->assertSuccessful();

        $run = BackupRun::first();

        $this->assertSame('succeeded', $run->status);

        Process::assertRanTimes(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'nice'
                && $process->command[1] === '-n'
                && $process->command[2] === '10'
                && $process->command[3] === 'proton-drive'
                && $process->command[4] === 'filesystem'
                && $process->command[5] === 'upload';
        }, 2);
    }

    #[Test]
    public function it_records_a_failed_run_when_the_upload_fails_twice(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::sequence()
                ->push(Process::result())
                ->push(Process::result())
                ->push(Process::result())
                ->push(Process::result(errorOutput: 'first upload error', exitCode: 1))
                ->push(Process::result(errorOutput: 'second upload error', exitCode: 1)),
        ]);

        $this->artisan('praefectus:run')->assertFailed();

        $run = BackupRun::first();

        $this->assertSame('failed', $run->status);

        $this->assertSame('second upload error', $run->error_summary);

        $this->assertNotNull($run->finished_at);

        Process::assertRanTimes(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'nice'
                && $process->command[1] === '-n'
                && $process->command[2] === '10'
                && $process->command[3] === 'proton-drive'
                && $process->command[4] === 'filesystem'
                && $process->command[5] === 'upload';
        }, 2);
    }

    #[Test]
    public function it_records_a_failed_run_when_archive_creation_fails(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::sequence()
                ->push(Process::result(errorOutput: 'tar failed', exitCode: 1)),
        ]);

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('[2/4] Creating compressed archive...')
            ->expectsOutputToContain('Backup archive creation failed.')
            ->doesntExpectOutputToContain('Archive created.')
            ->doesntExpectOutputToContain('[3/4]')
            ->doesntExpectOutputToContain('[4/4]')
            ->assertFailed();

        $run = BackupRun::first();

        $this->assertSame('failed', $run->status);

        $this->assertSame('tar failed', $run->error_summary);

        $this->assertNotNull($run->finished_at);

        Process::assertRan(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'tar';
        });

        Process::assertDidntRun(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && $process->command[0] === 'proton-drive';
        });
    }

    #[Test]
    public function it_records_a_clear_error_when_archive_creation_fails_without_diagnostics(): void
    {
        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
        ]);

        Process::preventStrayProcesses();
        Process::fake([
            '*' => Process::result(exitCode: 1),
        ]);

        $this->artisan('praefectus:run')->assertFailed();

        $run = BackupRun::first();

        $this->assertSame('failed', $run->status);
        $this->assertSame('Backup archive creation failed.', $run->error_summary);
        $this->assertNotNull($run->finished_at);
    }

    #[Test]
    public function it_records_a_skipped_run_when_no_files_match_the_profile_rules(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip', '*.txt', '*.md'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake();

        $this->artisan('praefectus:run')->assertSuccessful();

        $run = BackupRun::first();

        $this->assertSame('skipped', $run->status);

        $this->assertSame('No files matched backup profile rules.', $run->error_summary);

        $this->assertSame(0, $run->file_count);

        $this->assertNotNull($run->finished_at);

        Process::assertDidntRun(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && in_array($process->command[0], ['tar', 'proton-drive'], true);
        });
    }

    #[Test]
    public function it_skips_the_run_when_another_backup_is_already_running(): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
            'run_at' => '02:15',
        ]);

        Process::preventStrayProcesses();
        Process::fake();

        $lock = Cache::lock('praefectus:run', 600);

        $this->assertTrue($lock->get());

        try {
            $this->artisan('praefectus:run')->assertSuccessful();
        } finally {
            $lock->release();
        }

        $run = BackupRun::first();

        $this->assertSame('skipped', $run->status);

        $this->assertSame('Backup already running.', $run->error_summary);

        Process::assertDidntRun(function (PendingProcess $process, ProcessResult $result): bool {
            return is_array($process->command)
                && in_array($process->command[0], ['tar', 'proton-drive'], true);
        });
    }

    #[Test]
    public function it_records_a_failed_run_when_a_saved_source_directory_is_invalid(): void
    {
        BackupProfile::factory()->create([
            'source_directories' => ['/tmp/opencode/missing-praefectus-source'],
        ]);

        Process::preventStrayProcesses();
        Process::fake();

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('Source directory does not exist.')
            ->assertFailed();

        $run = BackupRun::first();

        $this->assertSame('failed', $run->status);
        $this->assertSame('Source directory does not exist.', $run->error_summary);
        $this->assertNotNull($run->finished_at);

        Process::assertDidntRun(function (PendingProcess $process, ProcessResult $result): bool {
            return true;
        });
    }

    #[Test]
    public function it_records_a_failed_run_when_a_backup_process_throws_an_exception(): void
    {
        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
        ]);

        Process::preventStrayProcesses();
        Process::fake(function (): never {
            throw new RuntimeException('The backup process failed unexpectedly.');
        });

        $this->artisan('praefectus:run')
            ->expectsOutputToContain('The backup process failed unexpectedly.')
            ->assertFailed();

        $run = BackupRun::first();

        $this->assertSame('failed', $run->status);
        $this->assertSame('The backup process failed unexpectedly.', $run->error_summary);
        $this->assertNotNull($run->finished_at);
    }

    #[Test]
    #[DataProvider('uploadOutcomes')]
    public function it_only_removes_local_files_after_a_successful_upload(bool $succeeds, bool $timesOut): void
    {
        $this->travelTo('2026-09-04 13:45:00');

        config([
            'praefectus.local_backup_path' => $this->sourceDirectory.'/backups',
            'praefectus.process_timeout' => 3600,
            'praefectus.upload_attempts' => 2,
        ]);

        BackupProfile::factory()->create([
            'source_directories' => [$this->sourceDirectory],
            'excluded_directory_names' => ['vendor'],
            'excluded_file_globs' => ['*.zip'],
        ]);

        $archivePath = $this->sourceDirectory.'/backups/backup-20260904-134500.tar.gz';
        $manifestPath = $this->sourceDirectory.'/backups/manifest-20260904-134500.txt';
        $uploadAttempts = 0;

        Process::preventStrayProcesses();
        Process::fake(function (PendingProcess $process) use ($succeeds, $timesOut, $archivePath, $manifestPath, &$uploadAttempts): ProcessResult {
            $this->assertSame(3600, $process->timeout);

            if ($process->command[0] === 'tar') {
                file_put_contents($archivePath, 'archive contents');
            }

            if (is_array($process->command) && in_array('upload', $process->command, true)) {
                $uploadAttempts++;
                $this->assertFileExists($archivePath);
                $this->assertFileExists($manifestPath);

                if ($uploadAttempts === 1 || ! $succeeds) {
                    if ($timesOut) {
                        throw new ProcessTimedOutException(
                            new SymfonyProcessTimedOutException(
                                new SymfonyProcess($process->command, timeout: 3600),
                                SymfonyProcessTimedOutException::TYPE_GENERAL,
                            ),
                            Process::result(exitCode: 1),
                        );
                    }

                    return Process::result(errorOutput: 'Upload failed.', exitCode: 1);
                }
            }

            return Process::result();
        });

        $command = $this->artisan('praefectus:run')
            ->expectsOutputToContain('Upload attempt 1 of 2...')
            ->expectsOutputToContain($timesOut
                ? 'Upload attempt timed out after 3600 seconds. Retrying...'
                : 'Upload attempt failed. Retrying...')
            ->expectsOutputToContain('Upload attempt 2 of 2...');

        if ($succeeds) {
            $command->expectsOutputToContain('Removing local backup files...')->assertSuccessful()->run();
            $this->assertFileDoesNotExist($archivePath);
            $this->assertFileDoesNotExist($manifestPath);
            $this->assertDirectoryDoesNotExist($this->sourceDirectory.'/backups');
        } else {
            $command->doesntExpectOutputToContain('Removing local backup files...')->assertFailed()->run();
            $this->assertFileExists($archivePath);
            $this->assertFileExists($manifestPath);
        }

        $this->assertSame(2, $uploadAttempts);
        $run = BackupRun::first();
        $this->assertSame($succeeds ? 'succeeded' : 'failed', $run->status);
        $this->assertSame(2, $run->file_count);
        $this->assertSame(strlen('archive contents'), $run->archive_size_bytes);
        $this->assertNotNull($run->finished_at);

        if (! $succeeds && $timesOut) {
            $this->assertStringContainsString('exceeded the timeout of 3600 seconds', $run->error_summary);
        }

        $lock = Cache::lock('praefectus:run', 600);
        $this->assertTrue($lock->get());
        $lock->release();
    }

    public static function uploadOutcomes(): array
    {
        return [
            'failed upload then success' => [true, false],
            'exhausted failed uploads' => [false, false],
            'timeout then success' => [true, true],
            'exhausted timeouts' => [false, true],
        ];
    }

    #[Test]
    public function it_prints_periodic_activity_while_external_processes_are_running(): void
    {
        BackupProfile::factory()->create(['source_directories' => [$this->sourceDirectory]]);
        Sleep::fake();
        $result = Process::result();

        $pending = Mockery::mock(PendingProcess::class);
        $pending->shouldReceive('start')->andReturnUsing(function () use ($result): InvokedProcess {
            $process = Mockery::mock(InvokedProcess::class);
            $iterations = 0;
            $process->shouldReceive('running')->andReturnUsing(function () use (&$iterations): bool {
                if ($iterations++ === 2) {
                    return false;
                }

                $this->travel(10)->seconds();

                return true;
            });
            $process->shouldReceive('ensureNotTimedOut')->twice();
            $process->shouldReceive('wait')->once()->andReturn($result);

            return $process;
        });
        Process::shouldReceive('timeout')->andReturn($pending);

        $this->artisan('praefectus:run', ['--no-ansi' => true])
            ->expectsOutputToContain('Creating archive: still running (10s elapsed).')
            ->expectsOutputToContain('Creating archive: still running (20s elapsed).')
            ->expectsOutputToContain('Uploading archive: still running (10s elapsed).')
            ->expectsOutputToContain('Uploading archive: still running (20s elapsed).')
            ->expectsOutputToContain('Backup completed successfully.')
            ->assertSuccessful();
    }

    #[Test]
    #[TestWith([0, 'completed'])]
    #[TestWith([1, 'failed'])]
    public function it_finishes_the_terminal_spinner_with_the_process_outcome(int $exitCode, string $outcome): void
    {
        BackupProfile::factory()->create(['source_directories' => [$this->sourceDirectory]]);
        Process::preventStrayProcesses();
        Process::fake(['*' => Process::result(exitCode: $exitCode)]);
        $output = new BufferedOutput(OutputInterface::VERBOSITY_NORMAL, true);

        $this->assertSame($exitCode, Artisan::call('praefectus:run', ['--ansi' => true], $output));

        $text = $output->fetch();
        $this->assertStringContainsString('Creating archive: '.$outcome, $text);
        $this->assertStringNotContainsString('still running', $text);

        if ($exitCode !== 0) {
            $this->assertStringNotContainsString('Creating archive: completed', $text);
            $this->assertStringNotContainsString('Backup completed successfully.', $text);
        }
    }

    private function deleteDirectory(string $directory): void
    {
        if (! is_dir($directory)) {
            return;
        }

        $items = scandir($directory);

        if ($items === false) {
            return;
        }

        foreach (array_diff($items, ['.', '..']) as $item) {
            $path = $directory.'/'.$item;

            if (is_dir($path)) {
                $this->deleteDirectory($path);

                continue;
            }

            unlink($path);
        }

        rmdir($directory);
    }
}
