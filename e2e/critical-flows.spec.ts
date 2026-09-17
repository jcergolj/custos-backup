import { execFileSync } from 'node:child_process';
import { closeSync, mkdirSync, openSync } from 'node:fs';
import path from 'node:path';
import { expect, type Page, test } from '@playwright/test';

const databasePath = path.resolve('database/e2e.sqlite');

test.beforeEach(() => {
    mkdirSync(path.dirname(databasePath), { recursive: true });
    closeSync(openSync(databasePath, 'w'));

    execFileSync('php', ['artisan', 'migrate:fresh', '--seed', '--no-interaction'], {
        stdio: 'ignore',
        env: {
            ...process.env,
            APP_ENV: 'local',
            APP_KEY: 'base64:8He8iwYO5TzXv3B8b+1D9Kj2ON9Yz2SrmjDGe6Qv7D8=',
            DB_CONNECTION: 'sqlite',
            DB_DATABASE: databasePath,
        },
    });
});

async function createAccount(page: Page): Promise<void> {
    await page.goto('/setup');

    await page.getByLabel('Name').fill('E2E Admin');
    await page.getByLabel('Email address').fill('e2e@example.com');
    await page.getByLabel('Password', { exact: true }).fill('password');
    await page.getByLabel('Confirm password').fill('password');
    await page.getByRole('button', { name: 'Create account' }).click();
}

test.describe('critical dashboard flows', () => {
    test('a new user can create the first account', async ({ page }) => {
        await createAccount(page);

        await expect(page).toHaveURL(/\/dashboard$/);
        await expect(page.getByText('Dashboard', { exact: true }).first()).toBeVisible();
        await expect(page.getByRole('button', { name: 'Back Up Now' })).toBeVisible();
    });

    test('an authenticated user can save a backup profile', async ({ page }) => {
        await createAccount(page);
        await page.getByRole('link', { name: 'Backup profile' }).first().click();

        await page.getByLabel('Source directories').fill('/tmp');
        await page.getByLabel('Excluded directory names').fill('node_modules\nvendor');
        await page.getByLabel('Excluded file globs').fill('.config/omarchy');
        await page.getByLabel('Daily backup time').fill('04:30');
        await page.getByRole('button', { name: 'Save' }).click();

        await expect(page).toHaveURL(/\/settings\/backup-profile\/edit$/);
        await expect(page.getByRole('alert')).toHaveText('Backup profile updated.');
        await expect(page.getByRole('alert')).toHaveClass(/bg-green-800/);
        await expect(page.getByText('Runs daily at 04:30')).toBeVisible();
        await expect(page.getByRole('listitem').filter({ hasText: '/tmp' })).toBeVisible();
        await expect(page.getByRole('listitem').filter({ hasText: 'node_modules' })).toBeVisible();
    });

    test('a user can log out and log back in to view the dashboard', async ({ page }) => {
        await createAccount(page);
        await page.goto('/settings');
        await page.locator('#settings-logout').evaluate((form) => (form as HTMLFormElement).submit());
        await expect(page).toHaveURL(/\/$/);

        await page.goto('/login');

        await page.getByLabel('Email address').fill('e2e@example.com');
        await page.getByLabel('Password').fill('password');
        await page.getByRole('button', { name: 'Log in' }).click();

        await expect(page).toHaveURL(/\/dashboard$/);
        await expect(page.getByText('Proton Drive storage')).toBeVisible();
        await expect(page.getByText('Total: Unavailable')).toBeVisible();
    });
});
