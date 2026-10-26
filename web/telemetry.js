import * as Sentry from '@sentry/browser';
import posthog from 'posthog-js/dist/module.slim';

// Public ingestion keys, never management/authentication credentials.
const enabled = location.hostname === 'road-rash.archit.xyz';
const release = typeof RR_WEB_RELEASE === 'string' ? RR_WEB_RELEASE : 'development';
const safe = operation => { try { return operation(); } catch (error) { console.warn('Tracking unavailable:', error); } };
const cleanUrl = value => {
    try { const url = new URL(value); return url.origin + url.pathname; } catch (_) { return undefined; }
};
if (enabled) {
    safe(() => Sentry.init({
        dsn: 'https://72f1b0670d21ecd90bc4908ebe82d35e@o425745.ingest.us.sentry.io/4512320659914752',
        environment: 'production', release, sendDefaultPii: false,
        // No console logs, replay, network breadcrumbs or interaction text.
        integrations: defaults => defaults.filter(i => i.name !== 'Breadcrumbs'),
        beforeSend(event) {
            delete event.user;
            if (event.request) {
                event.request.url = cleanUrl(event.request.url);
                delete event.request.headers; delete event.request.cookies; delete event.request.data;
            }
            for (const exception of event.exception?.values || []) {
                for (const frame of exception.stacktrace?.frames || []) {
                    if (/^https?:/.test(frame.filename)) frame.filename = cleanUrl(frame.filename);
                }
            }
            return event;
        }
    }));
    safe(() => posthog.init('phc_wnzWZVe4yh2bL979v7dyLTWZP7kYomNGC7myFqroyqmB', {
        api_host: 'https://us.i.posthog.com', defaults: '2026-08-30',
        autocapture: false, capture_pageview: false, capture_pageleave: false,
        capture_exceptions: false, disable_session_recording: true,
        disable_surveys: true, advanced_disable_flags: true,
        disable_external_dependency_loading: true,
        person_profiles: 'never', persistence: 'localStorage',
        save_campaign_params: false, save_referrer: false,
        property_denylist: ['$initial_referrer', '$initial_referring_domain'],
        before_send(event) {
            if (!event) return null;
            for (const key of ['$current_url', '$referrer', '$session_entry_url', '$session_entry_referrer', '$initial_current_url']) {
                if (event.properties[key]) event.properties[key] = cleanUrl(event.properties[key]);
            }
            return event;
        }
    }));
}
const allowed = new Set(['$pageview', 'launcher_ready', 'game_start', 'game_exit', 'launcher_exit',
    'connection_retry', 'disc_import_start', 'disc_import_complete', 'disc_import_cancelled', 'web_error', 'fullscreen']);
window.rrTelemetry = Object.freeze({
    track(name, properties = {}) {
        if (!enabled || !allowed.has(name)) return;
        // Deliberate allowlist: never send disc bytes, saves, filenames or input.
        const data = {release};
        for (const key of ['mode', 'course', 'source', 'phase', 'duration_ms']) {
            if (['string', 'number'].includes(typeof properties[key])) data[key] = properties[key];
        }
        safe(() => posthog.capture(name, data));
    },
    error(error, phase) {
        if (!enabled) return;
        safe(() => Sentry.captureException(error instanceof Error ? error : new Error(String(error)), {tags: {phase}}));
        this.track('web_error', {phase});
    }
});
window.rrTelemetry.track('$pageview');
