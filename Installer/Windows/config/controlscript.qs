// Controller script for Markdown Editor installer

function Controller()
{
    // Prevent the internal check from blocking with "TargetDirectoryInUse"
    // before our script has a chance to prompt the user.
    installer.setValue("RemoveTargetDir", false);
}

// Track whether we already asked to prevent popping up on every keystroke
Controller.alreadyPrompted = false;

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var widget = gui.currentPageWidget();
    if (widget != null) {
        // Connect to text changes if the user manually changes or browses for a path
        widget.TargetDirectoryLineEdit.textChanged.connect(this, Controller.prototype.checkExistingInstallation);

        // Run the check on entering the page
        Controller.prototype.checkExistingInstallation();
    }
}

Controller.prototype.checkExistingInstallation = function()
{
    // Retrieve target directory (check installer variable first, fallback to line edit)
    var dir = installer.value("TargetDir");
    var widget = gui.currentPageWidget();
    if (!dir && widget != null && widget.TargetDirectoryLineEdit) {
        dir = widget.TargetDirectoryLineEdit.text;
    }

    if (!dir || dir === "") {
        return;
    }

    // Normalize Windows backslashes to forward slashes
    dir = dir.replace(/\\/g, "/");

    // Determine maintenancetool binary path based on platform
    var maintTool = dir + "/maintenancetool.exe";
    if (systemInfo.productType === "osx" || systemInfo.productType === "macos") {
        maintTool = dir + "/maintenancetool.app/Contents/MacOS/maintenancetool";
    } else if (systemInfo.productType !== "windows") {
        maintTool = dir + "/maintenancetool";
    }

    // Check if an existing installation exists in the target directory
    if (installer.fileExists(maintTool) && !Controller.alreadyPrompted) {
        Controller.alreadyPrompted = true;

        // NOTE: Use a custom identifier so QtIFW does not suppress the dialog
		var reply = QMessageBox.question(
            "ConfirmOverwritePreviousInstallDialog",
            "Existing Installation Detected",
            "A previous installation of Markdown Editor was found in:<br><br>" +
            "<b>" + dir + "</b><br><br>" +
            "Would you like to uninstall the previous version and overwrite it?",
            QMessageBox.Yes | QMessageBox.No
        );

        if (reply === QMessageBox.Yes) {
            // Silently purge the old installation (removes old files and Windows registry keys)
            installer.execute(maintTool, ["purge", "-c"]);
        } else {
            // If the user chooses NO, re-enable RemoveTargetDir so the standard
            // protection prevents them from accidentally installing over the folder
            installer.setValue("RemoveTargetDir", true);

            QMessageBox.information(
                "SelectDifferentDirectoryNotice",
                "Different Directory Required",
                "Please choose a different folder for the new installation."
            );
        }
    }
}