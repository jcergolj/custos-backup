<?php

declare(strict_types=1);

namespace App\Http\Controllers\Settings;

use App\Http\Controllers\Controller;
use App\Http\Requests\SaveBackupProfileRequest;
use App\Models\BackupProfile;
use App\Services\BackupProfilePreview;
use Illuminate\Http\RedirectResponse;
use Illuminate\View\View;
use Jcergolj\InAppNotifications\Facades\InAppNotification;

class BackupProfileController extends Controller
{
    public function edit(BackupProfilePreview $preview): View
    {
        $profile = BackupProfile::firstOrNew([
            'id' => 1,
        ], [
            'source_directories' => [],
            'source_files' => [],
            'excluded_directory_names' => [],
            'excluded_file_globs' => [],
            'run_at' => '02:15',
            'schedule_frequency' => 'daily',
            'schedule_day' => 0,
        ]);

        return view('settings.backup-profile.edit', [
            'sourceDirectories' => $this->implodeLines($profile->source_directories),
            'sourceFiles' => $this->implodeLines($profile->source_files),
            'excludedDirectoryNames' => $this->implodeLines($profile->excluded_directory_names),
            'excludedFileGlobs' => $this->implodeLines($profile->excluded_file_globs),
            'configuredSourceDirectories' => is_array($profile->source_directories) ? $profile->source_directories : [],
            'configuredSourceFiles' => is_array($profile->source_files) ? $profile->source_files : [],
            'configuredExcludedDirectoryNames' => is_array($profile->excluded_directory_names) ? $profile->excluded_directory_names : [],
            'configuredExcludedFileGlobs' => is_array($profile->excluded_file_globs) ? $profile->excluded_file_globs : [],
            'runAt' => $profile->run_at,
            'scheduleFrequency' => $profile->schedule_frequency,
            'scheduleDay' => $profile->schedule_day ?? 0,
            'preview' => $preview->summarize($profile),
        ]);
    }

    public function update(SaveBackupProfileRequest $request): RedirectResponse
    {
        $profile = BackupProfile::firstOrNew(['id' => 1], ['run_at' => '02:15', 'schedule_frequency' => 'daily']);
        $profile->fill($request->validated());
        $profile->save();

        InAppNotification::success(__('Backup profile updated.'));

        return to_route('settings.backup-profile.edit');
    }

    private function implodeLines(mixed $value): string
    {
        if (! is_array($value)) {
            return '';
        }

        return implode(PHP_EOL, $value);
    }
}
