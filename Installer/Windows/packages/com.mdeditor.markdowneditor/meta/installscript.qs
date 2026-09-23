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

        // Application shortcut inside the Start Menu group from config.xml
        // (<StartMenuDir>Markdown Editor</StartMenuDir>).
        component.addOperation("CreateShortcut",
            appExe,
            "@StartMenuDir@/Markdown Editor.lnk",
            "iconPath=" + appExe,
            "iconId=0",
            "workingDirectory=@TargetDir@");

        // Maintenance tool shortcut (modify / repair / uninstall).
        var maint = "@TargetDir@/maintenancetool.exe";
        component.addOperation("CreateShortcut",
            maint,
            "@StartMenuDir@/Uninstall Markdown Editor.lnk",
            "iconPath=" + maint,
            "iconId=0",
            "workingDirectory=@TargetDir@");

        // Optional per-user desktop shortcut - uncomment to enable.
        // component.addOperation("CreateShortcut",
        //     appExe,
        //     "@DesktopDir@/Markdown Editor.lnk",
        //     "iconPath=" + appExe,
        //     "iconId=0",
        //     "workingDirectory=@TargetDir@");
    }
}
