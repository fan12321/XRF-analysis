#pragma once

#include <actions/GroupAction.h>

#include "EditSubsetAction.h"
#include "EditSubsetsAction.h"

class XRFAnalysisPlugin;

using namespace mv::gui;

class FunctionWidgetAction: public GroupAction {
public:
Q_INVOKABLE FunctionWidgetAction(QObject* parent, const QString& title);

    /**
     * Get the context menu for the action
     * @param parent Parent widget
     * @return Context menu
     */
    QMenu* getContextMenu(QWidget* parent = nullptr) override;

    /** Get reference to the image viewer plugin */
    XRFAnalysisPlugin& getXRFAnalysisPlugin() { return _XRFAnalysisPlugin; };

    EditSubsetAction& getEditSubsetAction() { return _editSubsetAction; };
    EditSubsetsAction& getEditSubsetsAction() { return _editSubsetsAction; };

public: // Action getters
    

protected:
    XRFAnalysisPlugin& _XRFAnalysisPlugin;     /** Reference to image viewer plugin */

    EditSubsetAction _editSubsetAction;
    EditSubsetsAction _editSubsetsAction;
};
