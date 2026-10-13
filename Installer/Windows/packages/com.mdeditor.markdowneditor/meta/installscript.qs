// Component script for com.mdeditor.markdowneditor.
// Adds Start Menu shortcuts on install; IFW removes them again on uninstall
// because CreateShortcut operations are transactional.

function Component()
{
}

Component.prototype.createOperations = function()
{
    // Default operations: extract files, write maintenance tool, etc.
    component.createOperations();

    if (systemInfo.productType === "windows") {
        var appExe = "@TargetDir@/markdowneditor.exe";

        // Application shortcut in Start Menu
        component.addOperation("CreateShortcut",
            appExe,
            "@StartMenuDir@/Markdown Editor.lnk",
            "iconPath=" + appExe,
            "iconId=0",
            "workingDirectory=@TargetDir@");

        // Maintenance tool shortcut
        var maint = "@TargetDir@/maintenancetool.exe";
        component.addOperation("CreateShortcut",
            maint,
            "@StartMenuDir@/Uninstall Markdown Editor.lnk",
            "iconPath=" + maint,
            "iconId=0",
            "workingDirectory=@TargetDir@");

        // -------------------------------------------------------------
        // Register file associations for "Open with..."
        // -------------------------------------------------------------
        // Command format: "path/to/markdowneditor.exe" "%1"
        var command = "\"" + appExe + "\" \"%1\"";

        // Register .md files
        component.addOperation("RegisterFileType",
            "md",                                     // extension (without dot)
            command,                                  // launch command
            "Markdown Document",                      // description
            "text/markdown",                          // MIME / content type
            appExe + ",0",                            // icon (icon 0 from your exe)
            "ProgId=com.mdeditor.markdowneditor.md"  // explicit ProgId
        );

        // Optional: Also register .markdown files
        component.addOperation("RegisterFileType",
            "markdown",
            command,
            "Markdown Document",
            "text/markdown",
            appExe + ",0",
            "ProgId=com.mdeditor.markdowneditor.md"
        );
    }
}