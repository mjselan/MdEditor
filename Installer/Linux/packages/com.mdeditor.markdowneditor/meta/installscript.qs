// Component script for the Linux com.mdeditor.markdowneditor package.
// The payload is extracted by IFW's default operations; this script ensures
// the binary is executable and registers a desktop entry so the app shows
// up in GNOME/KDE launchers. All operations are transactional, so uninstall
// removes the desktop entry together with the files.

function Component()
{
}

Component.prototype.createOperations = function()
{
    // Default operations: extract files, write maintenance tool, etc.
    component.createOperations();

    // Note: systemInfo.productType is the distro ID here (QSysInfo favors
    // "ubuntu", "debian", ... on Linux), so discriminate on the kernel.
    if (systemInfo.kernelType === "linux") {
        // The execute bit can be lost when the package archive is created
        // on some systems; make the launch deterministic.
        component.addOperation("Execute",
            "chmod", "+x", "@TargetDir@/markdowneditor");

        // Launcher entry in the user's applications directory. Icon points
        // at the staged PNG inside the install dir, so no hicolor install
        // or gtk-update-icon-cache step is needed.
        component.addOperation("CreateDesktopEntry",
            "@HomeDir@/.local/share/applications/markdowneditor.desktop",
            "Type=Application\n" +
            "Name=Markdown Editor\n" +
            "GenericName=Markdown Editor\n" +
            "Comment=Cross-platform Markdown editor with live preview\n" +
            "Exec=@TargetDir@/markdowneditor %F\n" +
            "Icon=@TargetDir@/markdowneditor.png\n" +
            "Terminal=false\n" +
            "Categories=Office;TextEditor;\n" +
            "MimeType=text/markdown;text/x-markdown;text/plain;\n" +
            "StartupWMClass=markdowneditor\n" +
            "Keywords=markdown;editor;notes;preview;\n");
    }
}
