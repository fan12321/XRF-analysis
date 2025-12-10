#include "FunctionWidgetAction.h"
#include "XRFAnalysisPlugin.h"

#include <QMenu>

using namespace mv::gui;

FunctionWidgetAction::FunctionWidgetAction(QObject* parent, const QString& title) :
    GroupAction(parent, title), 
    _XRFAnalysisPlugin(*static_cast<XRFAnalysisPlugin*>(parent)), 
    _editSubsetsAction(this, "Edit Subsets"),
    _editSubsetAction(this, "Edit Subset")
{
    setShowLabels(false);

    addAction(&_editSubsetsAction);
    addAction(&_editSubsetAction);

    _editSubsetAction.setStretch(1);
}


QMenu* FunctionWidgetAction::getContextMenu(QWidget* parent /*= nullptr*/)
{
    auto menu = new QMenu();

    const auto selectedRows = _XRFAnalysisPlugin.getSelectionModel().selectedRows();

    for (auto subset: _XRFAnalysisPlugin.getSubsetModel().getSubsets()) {
        auto subsetAction = new QAction("hi");

        subsetAction->setCheckable(true);
        subsetAction->setChecked(false);

        connect(subsetAction, &QAction::toggled, this, [this, subset](bool toggled) {
            // subset->getGeneralAction().getVisibleAction().setChecked(toggled);
            qDebug() << "subset toggled";
        });

        menu->addAction(subsetAction);
        qDebug() << "subsetAction added";
    }

    // menu->addSeparator();

    // menu->addAction(&_XRFAnalysisPlugin.getInteractionToolbarAction().getZoomExtentsAction());
    // menu->addAction(&_XRFAnalysisPlugin.getInteractionToolbarAction().getZoomSelectionAction());

    return menu;
}