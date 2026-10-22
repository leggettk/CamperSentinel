#pragma once

#include <Arduino.h>
#include "HtmlStream.h"
class WebServer;

enum class StatusLevel
{
    Normal,
    Warning,
    Critical,
    Neutral
};

class WebPageBuilder
{
public:
    explicit WebPageBuilder(WebServer& server) : html_(server) {}
    void begin(
        const String& title,
        const String& heading,
        const String& subtitle = "",
        uint16_t refreshSeconds = 0);

    void addNavigation(const String& activePage);

    void addHealthBanner(
        const String& title,
        const String& message,
        StatusLevel level);

    void beginGrid();
    void endGrid();

    void addMetricCard(
        const String& title,
        const String& value,
        const String& detail,
        StatusLevel level = StatusLevel::Neutral);

    void beginCard(const String& title);
    void endCard();

    void addStatus(
        const String& label,
        const String& value,
        StatusLevel level = StatusLevel::Neutral);

    void addValue(
        const String& label,
        const String& value);

    void addLink(
        const String& label,
        const String& url);

    void addFooter(
        const String& version,
        const String& uptime);
    
    void addSettingsCard(
        const String& title,
        const String& description,
        const String& href);

    void beginForm(
        const String& action,
        const String& method = "POST");

    void endForm();

    void addNumberInput(
        const String& label,
        const String& name,
        float value,
        float minimum,
        float maximum,
        float step,
        const String& unit = "",
        const String& description = "",
        uint8_t decimals = 1);

    void addCheckbox(
        const String& label,
        const String& name,
        bool checked,
        const String& description = "");

    void addSubmitButton(
        const String& label = "Save Changes");

    void addToast(
        const String& message,
        StatusLevel level = StatusLevel::Normal);    
    void send(WebServer& server);
    void addSettingsFormScript();
    void addTextInput(
    const String& label,
    const String& name,
    const String& value,
    size_t maxLength,
    const String& description = "");
    void addPasswordInput(
    const String& label,
    const String& name,
    size_t maxLength,
    const String& description = "");
    void addActionButton(
    const String& label,
    const String& action,
    bool danger = false);
private:
    HtmlStream html_;
    void finish();

    bool cardOpen_ = false;
    bool gridOpen_ = false;
    bool footerAdded_ = false;
    bool formOpen_ = false;
    void appendStyles();
    
    
    String escapeHtml(const String& value) const;
    String statusClass(StatusLevel level) const;
    String statusLabel(StatusLevel level) const;
};
