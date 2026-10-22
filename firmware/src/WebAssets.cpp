#include "WebAssets.h"
const char WEB_STYLES[] PROGMEM = R"css(
    :root {
        color-scheme: dark;
        font-family:
            -apple-system,
            BlinkMacSystemFont,
            "Segoe UI",
            Roboto,
            Arial,
            sans-serif;

        --background: #0b1015;
        --surface: #151c23;
        --surface-raised: #1b242d;
        --border: #29343f;
        --text: #f1f5f9;
        --muted: #94a3b2;

        --normal: #31d17c;
        --warning: #f5b942;
        --critical: #ff6262;
        --neutral: #7e8b98;
        --accent: #4c98ff;
    }

    * {
        box-sizing: border-box;
    }

    body {
        margin: 0;
        min-height: 100vh;
        background:
            radial-gradient(
                circle at top right,
                rgba(76, 152, 255, 0.12),
                transparent 32rem),
            var(--background);
        color: var(--text);
    }

    .container {
        width: min(100%, 960px);
        margin: 0 auto;
        padding: 28px 18px 44px;
    }

    .page-header h1 {
        margin: 0;
        font-size: clamp(2rem, 6vw, 3.1rem);
        line-height: 1.05;
        letter-spacing: -0.04em;
    }

    .subtitle {
        margin: 10px 0 0;
        color: var(--muted);
        font-size: 1rem;
    }

    .nav {
        display: flex;
        gap: 6px;
        margin: 24px 0;
        padding: 6px;
        overflow-x: auto;
        background: rgba(21, 28, 35, 0.88);
        border: 1px solid var(--border);
        border-radius: 13px;
    }

    .nav a {
        padding: 10px 14px;
        color: var(--muted);
        text-decoration: none;
        white-space: nowrap;
        border-radius: 9px;
        font-size: 0.9rem;
        font-weight: 650;
    }

    .nav a:hover,
    .nav a.active {
        background: var(--surface-raised);
        color: var(--text);
    }

    .health-banner {
        display: flex;
        align-items: center;
        gap: 16px;
        margin-bottom: 18px;
        padding: 20px;
        border: 1px solid var(--border);
        border-radius: 16px;
        background: var(--surface);
    }

    .health-icon {
        width: 18px;
        height: 18px;
        flex: 0 0 18px;
        border-radius: 50%;
    }

    .health-title {
        font-size: 1.1rem;
        font-weight: 750;
    }

    .health-message {
        margin-top: 4px;
        color: var(--muted);
        line-height: 1.4;
    }

    .health-banner.status-normal .health-icon {
        background: var(--normal);
        box-shadow: 0 0 18px rgba(49, 209, 124, 0.65);
    }

    .health-banner.status-warning .health-icon {
        background: var(--warning);
        box-shadow: 0 0 18px rgba(245, 185, 66, 0.55);
    }

    .health-banner.status-critical .health-icon {
        background: var(--critical);
        box-shadow: 0 0 18px rgba(255, 98, 98, 0.55);
    }

    .health-banner.status-neutral .health-icon {
        background: var(--neutral);
    }

    .metric-grid {
        display: grid;
        grid-template-columns:
            repeat(3, minmax(0, 1fr));
        gap: 14px;
        margin-bottom: 18px;
    }

    .metric-card {
        position: relative;
        overflow: hidden;
        min-height: 144px;
        padding: 19px;
        border: 1px solid var(--border);
        border-radius: 16px;
        background:
            linear-gradient(
                145deg,
                var(--surface-raised),
                var(--surface));
    }

    .metric-card::before {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 3px;
        content: "";
        background: var(--neutral);
    }

    .metric-card.status-normal::before {
        background: var(--normal);
    }

    .metric-card.status-warning::before {
        background: var(--warning);
    }

    .metric-card.status-critical::before {
        background: var(--critical);
    }

    .metric-title {
        color: var(--muted);
        font-size: 0.82rem;
        font-weight: 700;
        letter-spacing: 0.08em;
        text-transform: uppercase;
    }

    .metric-value {
        margin-top: 20px;
        font-size: clamp(1.65rem, 5vw, 2.4rem);
        font-weight: 780;
        letter-spacing: -0.035em;
    }

    .metric-detail {
        margin-top: 7px;
        color: var(--muted);
        font-size: 0.88rem;
    }

    .card {
        margin-bottom: 16px;
        padding: 20px;
        background: var(--surface);
        border: 1px solid var(--border);
        border-radius: 16px;
    }

    .card h2 {
        margin: 0 0 14px;
        color: var(--muted);
        font-size: 0.82rem;
        letter-spacing: 0.1em;
        text-transform: uppercase;
    }
    .settings-form {
    display: grid;
    gap: 18px;
}

.form-group {
    display: grid;
    gap: 8px;
}

.form-label {
    color: var(--text);
    font-weight: 700;
}

.form-description {
    margin-top: 5px;
    color: var(--muted);
    font-size: 0.88rem;
    line-height: 1.4;
}

.input-with-unit {
    display: flex;
    align-items: center;
    overflow: hidden;
    background: var(--background);
    border: 1px solid var(--border);
    border-radius: 10px;
}

.input-with-unit:focus-within {
    border-color: var(--accent);
    box-shadow: 0 0 0 3px rgba(76, 152, 255, 0.15);
}

.input-with-unit input {
    width: 100%;
    padding: 13px 14px;
    color: var(--text);
    background: transparent;
    border: 0;
    outline: 0;
    font: inherit;
}

.input-unit {
    padding: 0 14px;
    color: var(--muted);
    font-weight: 700;
}

.checkbox-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 20px;
    padding: 15px;
    background: var(--background);
    border: 1px solid var(--border);
    border-radius: 11px;
}

.checkbox-row input {
    width: 22px;
    height: 22px;
    accent-color: var(--accent);
}

.primary-button {
    width: 100%;
    padding: 14px 18px;
    color: white;
    background: var(--accent);
    border: 0;
    border-radius: 10px;
    font: inherit;
    font-weight: 750;
    cursor: pointer;
}

.primary-button:hover {
    filter: brightness(1.08);
}

.toast {
    margin-bottom: 16px;
    padding: 14px 16px;
    border: 1px solid var(--border);
    border-radius: 11px;
    background: var(--surface);
    font-weight: 700;
}

.toast.status-normal {
    color: var(--normal);
    border-color: rgba(49, 209, 124, 0.45);
}

.toast.status-warning {
    color: var(--warning);
    border-color: rgba(245, 185, 66, 0.45);
}

.toast.status-critical {
    color: var(--critical);
    border-color: rgba(255, 98, 98, 0.45);
}
    .status-row {
        display: flex;
        justify-content: space-between;
        align-items: center;
        gap: 18px;
        padding: 13px 0;
        border-bottom: 1px solid var(--border);
    }

    .status-row:first-child {
        padding-top: 0;
    }

    .status-row:last-child {
        padding-bottom: 0;
        border-bottom: none;
    }

    .status-label {
        color: #cbd5df;
    }

    .status-value {
        display: flex;
        align-items: center;
        justify-content: flex-end;
        gap: 9px;
        text-align: right;
        font-weight: 650;
    }

    .status-value.plain {
        color: var(--text);
    }

    .indicator {
        width: 9px;
        height: 9px;
        flex: 0 0 9px;
        border-radius: 50%;
        background: var(--neutral);
    }

    .indicator.status-normal {
        background: var(--normal);
        box-shadow: 0 0 8px rgba(49, 209, 124, 0.6);
    }

    .indicator.status-warning {
        background: var(--warning);
    }

    .indicator.status-critical {
        background: var(--critical);
    }

    a {
        color: #86baff;
    }
    .settings-grid {
    display: grid;
    gap: 14px;
}

.settings-card {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 20px;
    min-height: 104px;
    padding: 20px;
    color: var(--text);
    text-decoration: none;
    background:
        linear-gradient(
            145deg,
            var(--surface-raised),
            var(--surface));
    border: 1px solid var(--border);
    border-radius: 16px;
    transition:
        transform 0.15s ease,
        border-color 0.15s ease,
        box-shadow 0.15s ease;
}

.settings-card:hover {
    transform: translateY(-2px);
    border-color: rgba(76, 152, 255, 0.7);
    box-shadow: 0 10px 28px rgba(0, 0, 0, 0.22);
}

.settings-card-copy {
    min-width: 0;
}

.settings-title {
    font-size: 1.08rem;
    font-weight: 760;
}

.settings-description {
    margin-top: 7px;
    color: var(--muted);
    line-height: 1.45;
}

.settings-arrow {
    flex: 0 0 auto;
    color: var(--accent);
    font-size: 2rem;
    line-height: 1;
}
.primary-button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
    filter: none;
}

.form-message {
    min-height: 22px;
    color: var(--critical);
    font-size: 0.9rem;
    line-height: 1.4;
}
    .toast {
    transition:
        opacity 0.3s ease,
        transform 0.3s ease;
}
.text-input
{
    width: 100%;
    padding: 12px 14px;

    color: var(--text);

    background: var(--background);

    border: 1px solid var(--border);
    border-radius: 10px;

    font: inherit;
}

.text-input:focus
{
    outline: none;

    border-color: var(--accent);

    box-shadow:
        0 0 0 3px
        rgba(76,152,255,.15);
}
.toast-hidden {
    opacity: 0;
    transform: translateY(-8px);
}
    footer {
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 8px 18px;
        margin-top: 30px;
        color: #6f7d8b;
        text-align: center;
        font-size: 0.79rem;
    }

    footer strong {
        color: #93a0ad;
    }

    @media (max-width: 720px) {
        .metric-grid {
            grid-template-columns: 1fr;
        }

        .metric-card {
            min-height: 122px;
        }
    }

    @media (max-width: 480px) {
        .container {
            padding: 21px 13px 34px;
        }

        .status-row {
            align-items: flex-start;
        }

        .status-value {
            max-width: 58%;
        }

        .health-banner {
            padding: 17px;
        }
    }
    .action-form {
    margin: 16px 0 0;
}

.action-button {
    display: block;
    width: 100%;
    padding: 14px 18px;

    color: #ffffff;
    background: var(--accent);

    border: none;
    border-radius: 10px;

    font: inherit;
    font-size: 1rem;
    font-weight: 750;

    cursor: pointer;

    transition:
        filter 0.15s ease,
        transform 0.15s ease;
}

.action-button:hover {
    filter: brightness(1.08);
}

.action-button:active {
    transform: scale(0.99);
}

.action-button.danger {
    background: var(--critical);
}
)css";
