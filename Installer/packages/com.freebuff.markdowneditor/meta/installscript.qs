function Component()
{
    // installer default
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Windows shell wants backslash paths in IconLocation; IFW expands
        // @TargetDir@ with forward slashes which can leave shortcuts iconless.
        var exe = installer.value("TargetDir").replace(/\//g, "\\") + "\\markdowneditor.exe";

        component.addOperation("CreateShortcut",
            exe,
            installer.value("StartMenuDir") + "\\Markdown Editor.lnk",
            "workingDirectory=" + installer.value("TargetDir").replace(/\//g, "\\"),
            "iconPath=" + exe,
            "iconId=0",
            "description=Freebuff Markdown Editor");
        component.addOperation("CreateShortcut",
            exe,
            installer.value("DesktopDir") + "\\Markdown Editor.lnk",
            "workingDirectory=" + installer.value("TargetDir").replace(/\//g, "\\"),
            "iconPath=" + exe,
            "iconId=0",
            "description=Freebuff Markdown Editor");
    }
};
