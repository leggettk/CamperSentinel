#include "WebPageBuilder.h"
#include <WebServer.h>
#include "Logger.h"

void WebPageBuilder::begin(
    const String& title,
    const String& heading,
    const String& subtitle,
    uint16_t refreshSeconds)
{
    html_.start();
    cardOpen_ = false;
    gridOpen_ = false;
    footerAdded_ = false;
    formOpen_ = false;

    html_ += F("<!DOCTYPE html>");
    html_ += F("<html lang=\"en\">");
    html_ += F("<head>");
    html_ += F("<meta charset=\"UTF-8\">");
    html_ += F(
        "<meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1.0\">");

    if (refreshSeconds > 0)
    {
        html_ += F("<meta http-equiv=\"refresh\" content=\"");
        html_ += String(refreshSeconds);
        html_ += F("\">");
    }
    html_ += F("<title>");
    html_ += escapeHtml(title);
    html_ += F("</title>");

    appendStyles();

    html_ += F("</head>");
    html_ += F("<body>");
    html_ += F("<main class=\"container\">");

    html_ += F("<header class=\"page-header\">");
    html_ += F("<h1>");
    html_ += escapeHtml(heading);
    html_ += F("</h1>");

    if (subtitle.length() > 0)
    {
        html_ += F("<p class=\"subtitle\">");
        html_ += escapeHtml(subtitle);
        html_ += F("</p>");
    }

    html_ += F("</header>");
}

void WebPageBuilder::beginCard(const String& title)
{
    if (cardOpen_)
    {
        endCard();
    }

    html_ += F("<section class=\"card\">");
    html_ += F("<h2>");
    html_ += escapeHtml(title);
    html_ += F("</h2>");
    html_ += F("<div class=\"status-list\">");

    cardOpen_ = true;
}

void WebPageBuilder::endCard()
{
    if (!cardOpen_)
    {
        return;
    }

    html_ += F("</div>");
    html_ += F("</section>");

    cardOpen_ = false;
}

void WebPageBuilder::addStatus(
    const String& label,
    const String& value,
    StatusLevel level)
{
    if (!cardOpen_)
    {
        return;
    }

    const String levelClass = statusClass(level);

    html_ += F("<div class=\"status-row\">");

    html_ += F("<span class=\"status-label\">");
    html_ += escapeHtml(label);
    html_ += F("</span>");

    html_ += F("<span class=\"status-value\">");

    html_ += F("<span class=\"indicator ");
    html_ += levelClass;
    html_ += F("\"></span>");

    html_ += escapeHtml(value);
    html_ += F("</span>");

    html_ += F("</div>");
}


void WebPageBuilder::finish()
{
    if (formOpen_)
    {
        endForm();
    }
    if (gridOpen_)
    {
        endGrid();
    }

    if (cardOpen_)
    {
        endCard();
    }

    if (!footerAdded_)
    {
        html_ += F(
            "<footer>"
            "<strong>Camper Sentinel</strong>"
            "<span>Know your camper. Anywhere.</span>"
            "</footer>");
    }

    html_ += F("</main>");
    html_ += F("</body>");
    html_ += F("</html>");

    html_.finish();
}

void WebPageBuilder::appendStyles()
{
    html_ += F("<link rel=\"stylesheet\" href=\"/assets/style.css\">");
}

void WebPageBuilder::send(WebServer& server)
{
    (void)server; // The stream is bound to this request's server at construction.
    finish();
}

String WebPageBuilder::escapeHtml(const String& value) const
{
    String escaped;
    escaped.reserve(value.length() + 16);

    for (size_t index = 0; index < value.length(); ++index)
    {
        const char character = value[index];

        switch (character)
        {
            case '&':
                escaped += F("&amp;");
                break;

            case '<':
                escaped += F("&lt;");
                break;

            case '>':
                escaped += F("&gt;");
                break;

            case '"':
                escaped += F("&quot;");
                break;

            case '\'':
                escaped += F("&#39;");
                break;

            default:
                escaped += character;
                break;
        }
    }

    return escaped;
}
void WebPageBuilder::addNavigation(
    const String& activePage)
{
    html_ += F("<nav class=\"nav\">");

    struct NavItem
    {
        const char* label;
        const char* path;
        const char* id;
    };

    const NavItem items[] =
    {
        {"Dashboard", "/", "dashboard"},
        {"Settings", "/settings", "settings"},
        {"Diagnostics", "/diagnostics", "diagnostics"},
        {"Firmware", "/ota", "firmware"}
    };

    for (const NavItem& item : items)
    {
        html_ += F("<a href=\"");
        html_ += item.path;
        html_ += F("\"");

        if (activePage == item.id)
        {
            html_ += F(" class=\"active\"");
        }

        html_ += F(">");
        html_ += item.label;
        html_ += F("</a>");
    }

    html_ += F("</nav>");
}
void WebPageBuilder::addHealthBanner(
    const String& title,
    const String& message,
    StatusLevel level)
{
    const String levelClass = statusClass(level);

    html_ += F("<section class=\"health-banner ");
    html_ += levelClass;
    html_ += F("\">");

    html_ += F("<div class=\"health-icon\"></div>");

    html_ += F("<div>");
    html_ += F("<div class=\"health-title\">");
    html_ += escapeHtml(title);
    html_ += F("</div>");

    html_ += F("<div class=\"health-message\">");
    html_ += escapeHtml(message);
    html_ += F("</div>");
    html_ += F("</div>");

    html_ += F("</section>");
}
void WebPageBuilder::beginGrid()
{
    if (gridOpen_)
    {
        return;
    }

    html_ += F("<section class=\"metric-grid\">");
    gridOpen_ = true;
}

void WebPageBuilder::endGrid()
{
    if (!gridOpen_)
    {
        return;
    }

    html_ += F("</section>");
    gridOpen_ = false;
}
void WebPageBuilder::addMetricCard(
    const String& title,
    const String& value,
    const String& detail,
    StatusLevel level)
{
    const String levelClass = statusClass(level);

    html_ += F("<article class=\"metric-card ");
    html_ += levelClass;
    html_ += F("\">");

    html_ += F("<div class=\"metric-title\">");
    html_ += escapeHtml(title);
    html_ += F("</div>");

    html_ += F("<div class=\"metric-value\">");
    html_ += escapeHtml(value);
    html_ += F("</div>");

    html_ += F("<div class=\"metric-detail\">");
    html_ += escapeHtml(detail);
    html_ += F("</div>");

    html_ += F("</article>");
}
void WebPageBuilder::addValue(
    const String& label,
    const String& value)
{
    if (!cardOpen_)
    {
        return;
    }

    html_ += F("<div class=\"status-row\">");

    html_ += F("<span class=\"status-label\">");
    html_ += escapeHtml(label);
    html_ += F("</span>");

    html_ += F("<span class=\"status-value plain\">");
    html_ += escapeHtml(value);
    html_ += F("</span>");

    html_ += F("</div>");
}
void WebPageBuilder::addFooter(
    const String& version,
    const String& uptime)
{
    html_ += F("<footer>");

    html_ += F("<strong>Camper Sentinel ");
    html_ += escapeHtml(version);
    html_ += F("</strong>");

    html_ += F("<span>Know your camper. Anywhere.</span>");
    html_ += F("<span>Uptime ");
    html_ += escapeHtml(uptime);
    html_ += F("</span>");

    html_ += F("</footer>");

    footerAdded_ = true;
}
String WebPageBuilder::statusClass(
    StatusLevel level) const
{
    switch (level)
    {
        case StatusLevel::Normal:
            return "status-normal";

        case StatusLevel::Warning:
            return "status-warning";

        case StatusLevel::Critical:
            return "status-critical";

        case StatusLevel::Neutral:
        default:
            return "status-neutral";
    }
}

String WebPageBuilder::statusLabel(
    StatusLevel level) const
{
    switch (level)
    {
        case StatusLevel::Normal:
            return "Healthy";

        case StatusLevel::Warning:
            return "Warning";

        case StatusLevel::Critical:
            return "Critical";

        case StatusLevel::Neutral:
        default:
            return "Unknown";
    }
}
void WebPageBuilder::addSettingsCard(
    const String& title,
    const String& description,
    const String& href)
{
    html_ += F("<a class=\"settings-card\" href=\"");
    html_ += escapeHtml(href);
    html_ += F("\">");

    html_ += F("<div class=\"settings-card-copy\">");

    html_ += F("<div class=\"settings-title\">");
    html_ += escapeHtml(title);
    html_ += F("</div>");

    html_ += F("<div class=\"settings-description\">");
    html_ += escapeHtml(description);
    html_ += F("</div>");

    html_ += F("</div>");

    html_ += F("<span class=\"settings-arrow\" aria-hidden=\"true\">");
    html_ += F("&#8250;");
    html_ += F("</span>");

    html_ += F("</a>");
}
void WebPageBuilder::addLink(
    const String& label,
    const String& url)
{
    html_ += F("<p class=\"page-link\"><a href=\"");
    html_ += escapeHtml(url);
    html_ += F("\">");
    html_ += escapeHtml(label);
    html_ += F("</a></p>");
}
void WebPageBuilder::beginForm(
    const String& action,
    const String& method)
{
    if (formOpen_)
    {
        endForm();
    }

    html_ += F("<form class=\"settings-form\" action=\"");
    html_ += escapeHtml(action);
    html_ += F("\" method=\"");
    html_ += escapeHtml(method);
    html_ += F("\">");
    html_ += F("<form id=\"settingsForm\" class=\"settings-form\" action=\"");
    formOpen_ = true;
}

void WebPageBuilder::endForm()
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F("</form>");
    formOpen_ = false;
}
void WebPageBuilder::addNumberInput(
    const String& label,
    const String& name,
    float value,
    float minimum,
    float maximum,
    float step,
    const String& unit,
    const String& description,
    uint8_t decimals)
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F("<label class=\"form-group\">");

    html_ += F("<span class=\"form-label\">");
    html_ += escapeHtml(label);
    html_ += F("</span>");

    if (description.length() > 0)
    {
        html_ += F("<span class=\"form-description\">");
        html_ += escapeHtml(description);
        html_ += F("</span>");
    }

    html_ += F("<div class=\"input-with-unit\">");

    html_ += F("<input type=\"number\" name=\"");
    html_ += escapeHtml(name);
    html_ += F("\" value=\"");
    html_ += String(
        value,
        static_cast<unsigned int>(decimals));
    html_ += F("\" min=\"");
    html_ += String(minimum, 1);
    html_ += F("\" max=\"");
    html_ += String(maximum, 1);
    html_ += F("\" step=\"");
    html_ += String(step, 1);
    html_ += F("\" required>");

    if (unit.length() > 0)
    {
        html_ += F("<span class=\"input-unit\">");
        html_ += escapeHtml(unit);
        html_ += F("</span>");
    }

    html_ += F("</div>");
    html_ += F("</label>");
}
void WebPageBuilder::addCheckbox(
    const String& label,
    const String& name,
    bool checked,
    const String& description)
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F("<label class=\"checkbox-row\">");

    html_ += F("<div>");
    html_ += F("<div class=\"form-label\">");
    html_ += escapeHtml(label);
    html_ += F("</div>");

    if (description.length() > 0)
    {
        html_ += F("<div class=\"form-description\">");
        html_ += escapeHtml(description);
        html_ += F("</div>");
    }

    html_ += F("</div>");

    html_ += F("<input type=\"checkbox\" name=\"");
    html_ += escapeHtml(name);
    html_ += F("\" value=\"1\"");

    if (checked)
    {
        html_ += F(" checked");
    }

    html_ += F(">");

    html_ += F("</label>");
}
void WebPageBuilder::addSubmitButton(
    const String& label)
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F(
        "<button "
        "id=\"saveButton\" "
        "class=\"primary-button\" "
        "type=\"submit\">");

    html_ += escapeHtml(label);
    html_ += F("</button>");

    html_ += F(
        "<div id=\"formMessage\" "
        "class=\"form-message\" "
        "role=\"alert\"></div>");
}
void WebPageBuilder::addToast(
    const String& message,
    StatusLevel level)
{
    html_ += F("<div id=\"pageToast\" class=\"toast ");
    html_ += statusClass(level);
    html_ += F("\">");
    html_ += escapeHtml(message);
    html_ += F("</div>");

    html_ += F(R"rawliteral(
<script>
window.setTimeout(function() {
    const toast = document.getElementById("pageToast");

    if (!toast) {
        return;
    }

    toast.classList.add("toast-hidden");

    window.setTimeout(function() {
        toast.remove();
    }, 300);
}, 3000);
</script>
)rawliteral");
}

void WebPageBuilder::addTextInput(
    const String& label,
    const String& name,
    const String& value,
    size_t maxLength,
    const String& description)
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F("<label class=\"form-group\">");

    html_ += F("<span class=\"form-label\">");
    html_ += escapeHtml(label);
    html_ += F("</span>");

    if (!description.isEmpty())
    {
        html_ += F("<span class=\"form-description\">");
        html_ += escapeHtml(description);
        html_ += F("</span>");
    }

    html_ += F("<input type=\"text\" ");

    html_ += F("name=\"");
    html_ += escapeHtml(name);
    html_ += F("\" ");

    html_ += F("value=\"");
    html_ += escapeHtml(value);
    html_ += F("\" ");

    html_ += F("maxlength=\"");
    html_ += String(maxLength);
    html_ += F("\" ");

    html_ += F("class=\"text-input\">");

    html_ += F("</label>");
}

void WebPageBuilder::addSettingsFormScript()
{
    html_ += F(R"rawliteral(
<script>
document.addEventListener("DOMContentLoaded", function () {
    var form = document.getElementById("settingsForm");
    var button = document.getElementById("saveButton");
    var message = document.getElementById("formMessage");

    if (!form || !button || !message) {
        return;
    }

    function snapshot() {
        var result = [];
        var elements = form.elements;

        for (var i = 0; i < elements.length; i++) {
            var element = elements[i];

            if (!element.name) {
                continue;
            }

            if (element.type === "checkbox") {
                result.push(
                    element.name + "=" +
                    (element.checked ? "1" : "0"));
            } else {
                result.push(
                    element.name + "=" +
                    element.value);
            }
        }

        return result.join("&");
    }

    var initialSnapshot = snapshot();

    function validateForm() {
        var highInput =
            form.elements["tempHigh"];

        var resetInput =
            form.elements["tempReset"];

        if (!highInput || !resetInput) {
            message.textContent = "";
            return true;
        }

        var high = parseFloat(highInput.value);
        var reset = parseFloat(resetInput.value);

        if (isNaN(high) || isNaN(reset)) {
            message.textContent =
                "Enter valid temperature values.";
            return false;
        }

        if (high < 40 || high > 150) {
            message.textContent =
                "High temperature must be between 40°F and 150°F.";
            return false;
        }

        if (reset < 35) {
            message.textContent =
                "Reset temperature must be at least 35°F.";
            return false;
        }

        if (reset >= high) {
            message.textContent =
                "Reset temperature must be below the alarm temperature.";
            return false;
        }

        message.textContent = "";
        return true;
    }

    function updateButton() {
        var changed =
            snapshot() !== initialSnapshot;

        var valid =
            validateForm();

        button.disabled =
            !changed || !valid;
    }

    form.addEventListener(
        "input",
        updateButton);

    form.addEventListener(
        "change",
        updateButton);

    form.addEventListener(
        "submit",
        function (event) {
            if (!validateForm()) {
                event.preventDefault();
                updateButton();
                return;
            }

            button.disabled = true;
            button.textContent = "Saving...";
        });

    button.disabled = true;
});
</script>
)rawliteral");
}

void WebPageBuilder::addPasswordInput(
    const String& label,
    const String& name,
    size_t maxLength,
    const String& description)
{
    if (!formOpen_)
    {
        return;
    }

    html_ += F("<label class=\"form-group\">");

    html_ += F("<span class=\"form-label\">");
    html_ += escapeHtml(label);
    html_ += F("</span>");

    if (description.length() > 0)
    {
        html_ += F("<span class=\"form-description\">");
        html_ += escapeHtml(description);
        html_ += F("</span>");
    }

    html_ += F("<input type=\"password\" ");

    html_ += F("name=\"");
    html_ += escapeHtml(name);
    html_ += F("\" ");

    html_ += F("value=\"\" ");

    html_ += F("maxlength=\"");
    html_ += String(maxLength);
    html_ += F("\" ");

    html_ += F("autocomplete=\"new-password\" ");
    html_ += F("class=\"text-input\">");

    html_ += F("</label>");
}
void WebPageBuilder::addActionButton(
    const String& label,
    const String& action,
    bool danger)
{
    html_ += F("<form class=\"action-form\" method=\"POST\" action=\"");
    html_ += escapeHtml(action);
    html_ += F("\">");

    html_ += F("<button class=\"action-button");

    if (danger)
    {
        html_ += F(" danger");
    }

    html_ += F("\" type=\"submit\">");

    html_ += escapeHtml(label);

    html_ += F("</button>");
    html_ += F("</form>");
}
