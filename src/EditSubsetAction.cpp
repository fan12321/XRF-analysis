#include "EditSubsetAction.h"
#include "XRFAnalysisPlugin.h"

using namespace mv;
using namespace mv::gui;

EditSubsetAction::EditSubsetAction(QObject* parent, const QString& title) :
    GroupsAction(parent, title)
{
    setConnectionPermissionsToForceNone(true);

    auto plugin = static_cast<XRFAnalysisPlugin*>(parent->parent());

    const auto modelSelectionChanged = [this, plugin]() -> void {
        if (projects().isOpeningProject() || projects().isImportingProject())
            return;

        const auto selectedRows     = plugin->getSelectionModel().selectedRows();
        const auto hasSelection     = !selectedRows.isEmpty();
        const auto multiSelection   = selectedRows.count() >= 2;

        GroupsAction::GroupActions groupActions;

        if (hasSelection && !multiSelection) {
            auto subset = plugin->getSubsetModel().getSubsetFromIndex(selectedRows.first());

            Q_ASSERT(subset != nullptr);

            if (subset == nullptr)
                return;

            groupActions << subset;
            // groupActions << &subset->getImageSettingsAction();
            // groupActions << &subset->getSelectionAction();
            // groupActions << &subset->getSubsetAction();
        }
        setGroupActions(groupActions);
        
    };

    connect(&plugin->getSelectionModel(), &QItemSelectionModel::selectionChanged, this, modelSelectionChanged);
}