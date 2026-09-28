<?php

declare(strict_types=1);

namespace Tests\Feature\Http\Controllers\Settings;

use App\Http\Controllers\Settings\BackupProfileController;
use App\Http\Requests\SaveBackupProfileRequest;
use App\Models\BackupProfile;
use App\Models\User;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Jcergolj\FormRequestAssertions\TestableFormRequest;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\Test;
use Tests\TestCase;

#[CoversClass(BackupProfileController::class)]
class BackupProfileControllerTest extends TestCase
{
    use RefreshDatabase;
    use TestableFormRequest;

    #[Test]
    public function edit_has_auth_and_verified_middleware(): void
    {
        $user = User::factory()->create();

        $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

        $response->assertMiddlewareIsApplied('auth');

        $response->assertMiddlewareIsApplied('verified');
    }

    #[Test]
    public function edit_displays_backup_profile_form(): void
    {
        $user = User::factory()->create();

        $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

        $response->assertOk()
            ->assertViewIs('settings.backup-profile.edit')
            ->assertViewHasForm('id="update-backup-profile-form"', 'PUT', route('settings.backup-profile.update'))
            ->assertFormHasCSRF()
            ->assertSeeText('Source directories')
            ->assertSeeText('Excluded directory names')
            ->assertSeeText('Excluded file globs')
            ->assertSeeText('Daily backup time')
            ->assertSeeText('Use 24-hour format (HH:MM), for example 18:30.')
            ->assertSee('id="run_at"', false)
            ->assertSee('lang="en-GB"', false)
            ->assertFormHasSubmitButton();
    }

    #[Test]
    public function edit_displays_a_summary_of_the_saved_profile(): void
    {
        $user = User::factory()->create();

        BackupProfile::factory()->create([
            'source_directories' => [base_path('app'), base_path('storage')],
            'excluded_directory_names' => ['node_modules', 'vendor'],
            'excluded_file_globs' => ['*.zip'],
            'run_at' => '05:30',
        ]);

        $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

        $response->assertOk()
            ->assertSeeText('2 source directories configured')
            ->assertSeeText('2 excluded directory names')
            ->assertSeeText('1 excluded file glob')
            ->assertSeeText('Runs daily at 05:30')
            ->assertSeeText(base_path('app'))
            ->assertSeeText(base_path('storage'));
    }

    #[Test]
    public function edit_previews_included_excluded_and_symlinked_files(): void
    {
        $user = User::factory()->create();
        $directory = '/tmp/opencode/backup-preview-'.uniqid();
        mkdir($directory.'/excluded', 0777, true);
        file_put_contents($directory.'/keep.txt', 'keep');
        file_put_contents($directory.'/excluded/skip.txt', 'skip');
        symlink($directory.'/keep.txt', $directory.'/link.txt');

        try {
            BackupProfile::factory()->create([
                'source_directories' => [$directory],
                'excluded_directory_names' => ['excluded'],
            ]);

            $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

            $response->assertOk()
                ->assertSeeText('Contents preview')
                ->assertSeeText($directory.'/keep.txt')
                ->assertSeeText($directory.'/excluded')
                ->assertSeeText($directory.'/link.txt');
        } finally {
            unlink($directory.'/link.txt');
            unlink($directory.'/excluded/skip.txt');
            rmdir($directory.'/excluded');
            unlink($directory.'/keep.txt');
            rmdir($directory);
        }
    }

    #[Test]
    public function edit_displays_saved_exclude_rules_in_the_summary(): void
    {
        $user = User::factory()->create();

        BackupProfile::factory()->create([
            'source_directories' => [base_path('app')],
            'excluded_directory_names' => ['node_modules', 'vendor'],
            'excluded_file_globs' => ['*.zip', '*.mp4'],
            'run_at' => '05:30',
        ]);

        $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

        $response->assertOk()
            ->assertSeeText('Configured excluded directory names')
            ->assertSeeText('node_modules')
            ->assertSeeText('vendor')
            ->assertSeeText('Configured excluded file globs')
            ->assertSeeText('*.zip')
            ->assertSeeText('*.mp4');
    }

    #[Test]
    public function edit_prefills_the_form_with_the_saved_profile_values(): void
    {
        $user = User::factory()->create();

        BackupProfile::factory()->create([
            'source_directories' => [base_path('app'), base_path('storage')],
            'excluded_directory_names' => ['node_modules', 'vendor'],
            'excluded_file_globs' => ['*.zip', '*.mp4'],
            'run_at' => '05:30',
        ]);

        $response = $this->actingAs($user)->get(route('settings.backup-profile.edit'));

        $response->assertOk()
            ->assertViewHasForm('id="update-backup-profile-form"', 'PUT', route('settings.backup-profile.update'))
            ->assertFormHasTimeInput('run_at', '05:30');

        $response->assertSeeInOrder([
            'id="source_directories"',
            base_path('app'),
            base_path('storage'),
            '</textarea>',
        ], false);

        $response->assertSeeInOrder([
            'id="excluded_directory_names"',
            'node_modules',
            'vendor',
            '</textarea>',
        ], false);

        $response->assertSeeInOrder([
            'id="excluded_file_globs"',
            '*.zip',
            '*.mp4',
            '</textarea>',
        ], false);
    }

    #[Test]
    public function update_has_form_request(): void
    {
        $this->put(route('settings.backup-profile.update'));

        $this->assertContainsFormRequest(SaveBackupProfileRequest::class);
    }

    #[Test]
    public function update_saves_the_backup_profile(): void
    {
        $user = User::factory()->create();

        $response = $this->actingAs($user)
            ->from(route('settings.backup-profile.edit'))
            ->put(route('settings.backup-profile.update'), [
                'source_directories' => base_path('app')."\n".base_path('storage'),
                'excluded_directory_names' => "node_modules\nvendor",
                'excluded_file_globs' => "*.zip\n*.mp4",
                'run_at' => '02:15',
            ]);

        $response->assertRedirect(route('settings.backup-profile.edit'));

        $profile = BackupProfile::first();

        $this->assertSame([base_path('app'), base_path('storage')], $profile->source_directories);

        $this->assertSame(['node_modules', 'vendor'], $profile->excluded_directory_names);

        $this->assertSame(['*.zip', '*.mp4'], $profile->excluded_file_globs);

        $this->assertSame('02:15', $profile->run_at);
    }

    #[Test]
    public function update_rejects_source_directories_that_do_not_exist(): void
    {
        $user = User::factory()->create();

        $response = $this->actingAs($user)
            ->from(route('settings.backup-profile.edit'))
            ->put(route('settings.backup-profile.update'), [
                'source_directories' => '/tmp/opencode/backup-profile-missing-directory',
                'excluded_directory_names' => '',
                'excluded_file_globs' => '',
                'run_at' => '02:15',
            ]);

        $response->assertRedirect(route('settings.backup-profile.edit'))
            ->assertSessionHasErrors(['source_directories.0']);
    }

    #[Test]
    public function update_rejects_relative_source_directories_with_a_clear_message(): void
    {
        $user = User::factory()->create();

        $response = $this->followingRedirects()
            ->actingAs($user)
            ->from(route('settings.backup-profile.edit'))
            ->put(route('settings.backup-profile.update'), [
                'source_directories' => "relative/path\n".base_path('app'),
                'excluded_directory_names' => '',
                'excluded_file_globs' => '',
                'run_at' => '02:15',
            ]);

        $response->assertSeeText('Backup profile')
            ->assertSeeText('Source directories must use absolute paths.');
    }
}
