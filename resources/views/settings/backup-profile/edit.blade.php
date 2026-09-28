@php($sourceDirectoriesValue = old('source_directories', $sourceDirectories))
@php($sourceFilesValue = old('source_files', $sourceFiles))
@php($excludedDirectoryNamesValue = old('excluded_directory_names', $excludedDirectoryNames))
@php($excludedFileGlobsValue = old('excluded_file_globs', $excludedFileGlobs))
@php($sourceDirectoryCount = count($configuredSourceDirectories))
@php($sourceFileCount = count($configuredSourceFiles))
@php($excludedDirectoryNameCount = count($configuredExcludedDirectoryNames))
@php($excludedFileGlobCount = count($configuredExcludedFileGlobs))

<x-layouts.app :title="__('Backup profile')">
    <section class="mx-auto w-full lg:max-w-2xl">
        <x-back-link :href="route('settings.index')">{{ __('Profile & Settings') }}</x-back-link>
        <x-text.heading size="xl">{{ __('Backup profile') }}</x-text.heading>

        <x-text.subheading>{{ __('Choose what should be included in scheduled backups on this computer.') }}</x-text.subheading>

        <x-page-card class="my-6 space-y-4">
            <x-text.heading size="lg">{{ __('Current profile') }}</x-text.heading>

            <div class="grid gap-3 md:grid-cols-2">
                <div>
                    <x-text class="font-medium">{{ trans_choice('{1} :count source directory configured|[2,*] :count source directories configured', $sourceDirectoryCount, ['count' => $sourceDirectoryCount]) }}</x-text>
                    <x-text class="mt-1">{{ $scheduleFrequency === 'weekly' ? __('Runs weekly on :day at :time', ['day' => ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'][$scheduleDay], 'time' => $runAt]) : __('Runs daily at :time', ['time' => $runAt]) }}</x-text>
                </div>

                <div>
                    <x-text class="font-medium">{{ trans_choice('{1} :count excluded directory name|[2,*] :count excluded directory names', $excludedDirectoryNameCount, ['count' => $excludedDirectoryNameCount]) }}</x-text>
                    <x-text class="mt-1">{{ trans_choice('{1} :count excluded file glob|[2,*] :count excluded file globs', $excludedFileGlobCount, ['count' => $excludedFileGlobCount]) }}</x-text>
                </div>
            </div>

            <x-text class="font-medium">{{ trans_choice('{1} :count source file configured|[2,*] :count source files configured', $sourceFileCount, ['count' => $sourceFileCount]) }}</x-text>

            @if ($sourceDirectoryCount > 0)
                <div>
                    <x-text class="font-medium">{{ __('Configured source directories') }}</x-text>

                    <ul class="mt-2 space-y-2">
                        @foreach ($configuredSourceDirectories as $directory)
                            <li class="rounded-box border border-base-300 bg-base-100 px-3 py-2 text-sm break-all">{{ $directory }}</li>
                        @endforeach
                    </ul>
                </div>
            @endif

            @if ($sourceFileCount > 0)
                <div>
                    <x-text class="font-medium">{{ __('Configured source files') }}</x-text>
                    <ul class="mt-2 space-y-2">
                        @foreach ($configuredSourceFiles as $file)
                            <li class="rounded-box border border-base-300 bg-base-100 px-3 py-2 text-sm break-all">{{ $file }}</li>
                        @endforeach
                    </ul>
                </div>
            @endif

            @if ($excludedDirectoryNameCount > 0)
                <div>
                    <x-text class="font-medium">{{ __('Configured excluded directory names') }}</x-text>

                    <ul class="mt-2 space-y-2">
                        @foreach ($configuredExcludedDirectoryNames as $directoryName)
                            <li class="rounded-box border border-base-300 bg-base-100 px-3 py-2 text-sm break-all">{{ $directoryName }}</li>
                        @endforeach
                    </ul>
                </div>

            @endif

            @if ($excludedFileGlobCount > 0)
                <div>
                    <x-text class="font-medium">{{ __('Configured excluded file globs') }}</x-text>

                    <ul class="mt-2 space-y-2">
                        @foreach ($configuredExcludedFileGlobs as $fileGlob)
                            <li class="rounded-box border border-base-300 bg-base-100 px-3 py-2 text-sm break-all">{{ $fileGlob }}</li>
                        @endforeach
                    </ul>
                </div>
            @endif
        </x-page-card>

        <x-page-card class="my-6 space-y-4">
            <x-text.heading size="lg">{{ __('Contents preview') }}</x-text.heading>
            <x-text>{{ __('This preview uses the saved profile and shows what the next backup can see.') }}</x-text>

            @foreach (['included' => 'Included files', 'excluded' => 'Excluded paths', 'skipped' => 'Skipped links or unsupported entries', 'missing' => 'Missing sources'] as $key => $label)
                <div>
                    <x-text class="font-medium">{{ __($label) }} ({{ count($preview[$key]) }})</x-text>
                    @if ($preview[$key] !== [])
                        <ul class="mt-2 space-y-1 text-sm">
                            @foreach ($preview[$key] as $path)
                                <li class="break-all">{{ $path }}</li>
                            @endforeach
                        </ul>
                    @endif
                </div>
            @endforeach
        </x-page-card>

        <x-page-card class="my-6">
            <form
                id="update-backup-profile-form"
                action="{{ route('settings.backup-profile.update') }}"
                method="post"
                class="space-y-6"
                data-controller="bridge--form"
                data-action="turbo:submit-start->bridge--form#submitStart turbo:submit-end->bridge--form#submitEnd"
            >
                @csrf
                @method('put')

                <div>
                    <x-form.label for="schedule_frequency">{{ __('Backup frequency') }}</x-form.label>
                    <select id="schedule_frequency" name="schedule_frequency" class="mt-2 w-full select">
                        <option value="daily" @selected(old('schedule_frequency', $scheduleFrequency) === 'daily')>{{ __('Daily') }}</option>
                        <option value="weekly" @selected(old('schedule_frequency', $scheduleFrequency) === 'weekly')>{{ __('Weekly') }}</option>
                    </select>
                    <x-form.error for="schedule_frequency" />
                </div>

                <div>
                    <x-form.label for="schedule_day">{{ __('Weekly day') }}</x-form.label>
                    <select id="schedule_day" name="schedule_day" class="mt-2 w-full select">
                        @foreach (['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'] as $day)
                            <option value="{{ $loop->index }}" @selected((int) old('schedule_day', $scheduleDay) === $loop->index)>{{ __($day) }}</option>
                        @endforeach
                    </select>
                    <x-form.error for="schedule_day" />
                </div>

                <div>
                    <x-form.label for="source_directories">{{ __('Source directories') }}</x-form.label>
                    <x-text class="mt-1">{{ __('Enter one absolute path per line.') }}</x-text>

                    <textarea
                        id="source_directories"
                        name="source_directories"
                        rows="5"
                        class="mt-2 w-full textarea data-error:textarea-error"
                        @if($errors->has('source_directories') || $errors->has('source_directories.0')) data-error="true" @endif
                    >{{ is_array($sourceDirectoriesValue) ? implode(PHP_EOL, $sourceDirectoriesValue) : $sourceDirectoriesValue }}</textarea>

                    <x-form.error for="source_directories" />
                    <x-form.error for="source_directories.0" />
                </div>

                <div>
                    <x-form.label for="source_files">{{ __('Source files') }}</x-form.label>
                    <x-text class="mt-1">{{ __('Enter one absolute file path per line.') }}</x-text>
                    <textarea id="source_files" name="source_files" rows="4" class="mt-2 w-full textarea data-error:textarea-error">{{ is_array($sourceFilesValue) ? implode(PHP_EOL, $sourceFilesValue) : $sourceFilesValue }}</textarea>
                    <x-form.error for="source_files" />
                    <x-form.error for="source_files.0" />
                </div>

                <div>
                    <x-form.label for="excluded_directory_names">{{ __('Excluded directory names') }}</x-form.label>
                    <x-text class="mt-1">{{ __('Enter folder names such as node_modules or vendor, one per line.') }}</x-text>

                    <textarea
                        id="excluded_directory_names"
                        name="excluded_directory_names"
                        rows="4"
                        class="mt-2 w-full textarea data-error:textarea-error"
                        @if($errors->has('excluded_directory_names')) data-error="true" @endif
                    >{{ is_array($excludedDirectoryNamesValue) ? implode(PHP_EOL, $excludedDirectoryNamesValue) : $excludedDirectoryNamesValue }}</textarea>

                    <x-form.error for="excluded_directory_names" />
                </div>

                <div>
                    <x-form.label for="excluded_file_globs">{{ __('Excluded file globs') }}</x-form.label>
                    <x-text class="mt-1">{{ __('Enter glob patterns such as *.zip or *.mp4, one per line.') }}</x-text>

                    <textarea
                        id="excluded_file_globs"
                        name="excluded_file_globs"
                        rows="4"
                        class="mt-2 w-full textarea data-error:textarea-error"
                        @if($errors->has('excluded_file_globs')) data-error="true" @endif
                    >{{ is_array($excludedFileGlobsValue) ? implode(PHP_EOL, $excludedFileGlobsValue) : $excludedFileGlobsValue }}</textarea>

                    <x-form.error for="excluded_file_globs" />
                </div>

                <div>
                    <x-form.label for="run_at">{{ __('Daily backup time') }}</x-form.label>
                    <x-text class="mt-1">{{ __('Use 24-hour format (HH:MM), for example 18:30.') }}</x-text>

                    <x-form.text-input
                        id="run_at"
                        type="time"
                        lang="en-GB"
                        name="run_at"
                        :value="old('run_at', $runAt)"
                        :data-error="$errors->has('run_at')"
                        required
                        class="mt-2"
                    />

                    <x-form.error for="run_at" />
                </div>

                <div class="flex items-center justify-end">
                    <x-form.button.primary
                        type="submit"
                        class="w-full"
                        data-bridge--form-target="submit"
                        data-bridge-title="{{ __('Save') }}"
                    >
                        {{ __('Save') }}
                    </x-form.button.primary>
                </div>
            </form>
        </x-page-card>
    </section>
</x-layouts.app>
