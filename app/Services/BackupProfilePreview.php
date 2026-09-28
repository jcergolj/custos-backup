<?php

declare(strict_types=1);

namespace App\Services;

use App\Models\BackupProfile;
use RecursiveCallbackFilterIterator;
use RecursiveDirectoryIterator;
use RecursiveIteratorIterator;
use SplFileInfo;

class BackupProfilePreview
{
    /** @return array<int, string> */
    public function includedFiles(BackupProfile $profile): array
    {
        return $this->summarize($profile)['included'];
    }

    /** @return array{included: array<int, string>, excluded: array<int, string>, skipped: array<int, string>, missing: array<int, string>} */
    public function summarize(BackupProfile $profile): array
    {
        $preview = [
            'included' => [],
            'excluded' => [],
            'skipped' => [],
            'missing' => [],
        ];
        $excludedDirectoryNames = $this->stringList($profile->excluded_directory_names);
        $excludedFileGlobs = $this->stringList($profile->excluded_file_globs);

        foreach ($this->stringList($profile->source_files) as $sourceFile) {
            if (is_link($sourceFile)) {
                $preview['skipped'][] = $sourceFile;
            } elseif (! is_file($sourceFile)) {
                $preview['missing'][] = $sourceFile;
            } elseif ($this->isIncludedFile($sourceFile, $excludedFileGlobs)) {
                $preview['included'][] = $sourceFile;
            } else {
                $preview['excluded'][] = $sourceFile;
            }
        }

        foreach ($this->stringList($profile->source_directories) as $sourceDirectory) {
            if (! is_dir($sourceDirectory) || is_link($sourceDirectory)) {
                $preview[is_link($sourceDirectory) ? 'skipped' : 'missing'][] = $sourceDirectory;

                continue;
            }

            $iterator = new RecursiveIteratorIterator(
                new RecursiveCallbackFilterIterator(
                    new RecursiveDirectoryIterator($sourceDirectory, RecursiveDirectoryIterator::SKIP_DOTS),
                    function (SplFileInfo $file) use ($excludedDirectoryNames, $excludedFileGlobs, $sourceDirectory, &$preview): bool {
                        if ($file->isLink()) {
                            $preview['skipped'][] = $file->getPathname();

                            return false;
                        }

                        if ($file->isDir()) {
                            if (in_array($file->getFilename(), $excludedDirectoryNames, true)) {
                                $preview['excluded'][] = $file->getPathname();

                                return false;
                            }

                            return true;
                        }

                        if (! $file->isFile()) {
                            $preview['skipped'][] = $file->getPathname();

                            return false;
                        }

                        $bucket = $this->isIncludedFile($file->getPathname(), $excludedFileGlobs, $sourceDirectory)
                            ? 'included'
                            : 'excluded';
                        $preview[$bucket][] = $file->getPathname();

                        return false;
                    },
                ),
            );

            foreach ($iterator as $_) {
            }
        }

        foreach ($preview as &$paths) {
            $paths = array_values(array_unique($paths));
            sort($paths);
        }

        return $preview;
    }

    /** @param array<int, string> $excludedFileGlobs */
    private function isIncludedFile(string $path, array $excludedFileGlobs, ?string $sourceDirectory = null): bool
    {
        $normalizedPath = str_replace(DIRECTORY_SEPARATOR, '/', $path);
        $relativePath = $sourceDirectory === null
            ? null
            : ltrim(str_replace(DIRECTORY_SEPARATOR, '/', substr($path, strlen($sourceDirectory))), '/');

        foreach ($excludedFileGlobs as $excludedFileGlob) {
            $normalizedGlob = preg_replace('#^\./#', '', str_replace(DIRECTORY_SEPARATOR, '/', trim($excludedFileGlob))) ?? $excludedFileGlob;

            foreach (array_filter([basename($normalizedPath), $normalizedPath, $relativePath]) as $candidate) {
                if (fnmatch($normalizedGlob, $candidate)
                    || str_starts_with($candidate, rtrim($normalizedGlob, '/').'/')) {
                    return false;
                }
            }
        }

        return true;
    }

    /** @return array<int, string> */
    private function stringList(mixed $value): array
    {
        return is_array($value)
            ? array_values(array_filter($value, fn (mixed $item): bool => is_string($item) && $item !== ''))
            : [];
    }
}
