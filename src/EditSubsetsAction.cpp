#include "EditSubsetsAction.h"
#include "FunctionWidgetAction.h"
#include "XRFAnalysisPlugin.h"

#include <Application.h>
// #include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>
#include <widgets/Divider.h>

#include <QTreeView>
#include <QHeaderView>

#include "Subset.h"

using namespace mv;
using namespace mv::gui;

QRandomGenerator EditSubsetsAction::rng;

EditSubsetsAction::EditSubsetsAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
    _functionWidgetAction(*static_cast<FunctionWidgetAction*>(parent)),
    _removeSubsetAction(this, "Remove"),
    _moveSubsetToTopAction(this, "Move To Top"),
    _moveSubsetUpAction(this, "Move Up"),
    _moveSubsetDownAction(this, "Move Down"),
    _moveSubsetToBottomAction(this, "Move To Bottom"),
    _setSelectionAction(this, "Set Selection"),
    _exportClusterAction(this, "Export Cluster"),
    _operationsAction(this, "Operations")
{
    setConnectionPermissionsToForceNone(true);

    _operationsAction.setDefaultWidgetFlags(GroupAction::Horizontal);

    _operationsAction.addAction(&_removeSubsetAction, TriggerAction::Icon);
    _operationsAction.addAction(&_moveSubsetToTopAction, TriggerAction::Icon);
    _operationsAction.addAction(&_moveSubsetUpAction, TriggerAction::Icon);
    _operationsAction.addAction(&_moveSubsetDownAction, TriggerAction::Icon);
    _operationsAction.addAction(&_moveSubsetToBottomAction, TriggerAction::Icon);
    _operationsAction.addAction(&_setSelectionAction, TriggerAction::Icon);
    _operationsAction.addAction(&_exportClusterAction, TriggerAction::Icon);
    _operationsAction.addStretch(1);

    _removeSubsetAction.setToolTip("Remove the selected subset");
    _moveSubsetToTopAction.setToolTip("Move the selected subset to the top");
    _moveSubsetUpAction.setToolTip("Move the selected subset up");
    _moveSubsetDownAction.setToolTip("Move the selected subset down");
    _moveSubsetToBottomAction.setToolTip("Move the selected subset to the bottom");
    _setSelectionAction.setToolTip("Set the selected subset as the current selection");
    _exportClusterAction.setToolTip("Export the subsets to a cluster dataset");

    _removeSubsetAction.setIconByName("trash-alt");
    _moveSubsetToTopAction.setIconByName("angle-double-up");
    _moveSubsetUpAction.setIconByName("angle-up");
    _moveSubsetDownAction.setIconByName("angle-down");
    _moveSubsetToBottomAction.setIconByName("angle-double-down");
    _setSelectionAction.setIconByName("check-square");
    _exportClusterAction.setIconByName("file-export");

}

QColor EditSubsetsAction::getRandomSubsetColor()
{
    const auto randomHue        = rng.bounded(360);
    const auto randomSaturation = rng.bounded(150, 255);
    const auto randomLightness  = rng.bounded(150, 220);

    return QColor::fromHsl(randomHue, randomSaturation, randomLightness);
}


EditSubsetsAction::Widget::Widget(QWidget* parent, EditSubsetsAction* editSubsetsAction) :
    WidgetActionWidget(parent, editSubsetsAction),
    _hierarchyWidget(this, "Subset", editSubsetsAction->getFunctionWidgetAction().getXRFAnalysisPlugin().getSubsetModel(), nullptr, false)
{
    auto& plugin = editSubsetsAction->getFunctionWidgetAction().getXRFAnalysisPlugin();

    _hierarchyWidget.setWindowIcon(StyledIcon("layer-group"));
    
    auto& treeView = _hierarchyWidget.getTreeView();

    treeView.setSelectionModel(&plugin.getSelectionModel());
    // treeView.setModel(dynamic_cast<QAbstractItemModel*>(&plugin.getSelectionModel()));
    treeView.setRootIsDecorated(false);
    treeView.setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    treeView.setSelectionMode(QAbstractItemView::SelectionMode::ContiguousSelection);
    
    auto treeViewHeader = treeView.header();

    treeViewHeader->setStretchLastSection(false);

    treeViewHeader->setSectionResizeMode(static_cast<int>(SubsetModel::Column::Visible), QHeaderView::Fixed);
    treeViewHeader->setSectionResizeMode(static_cast<int>(SubsetModel::Column::Color), QHeaderView::Fixed);
    treeViewHeader->setSectionResizeMode(static_cast<int>(SubsetModel::Column::Name), QHeaderView::Stretch);

    const auto toggleColumnSize = 16;

    treeViewHeader->resizeSection(static_cast<int>(SubsetModel::Column::Visible), toggleColumnSize);
    treeViewHeader->resizeSection(static_cast<int>(SubsetModel::Column::Color), toggleColumnSize);

    connect(&_hierarchyWidget.getTreeView(), &QTreeView::clicked, this, [this](const QModelIndex& index) -> void {
        static const QVector<int> toggleColumns = {
            static_cast<int>(SubsetModel::Column::Visible)
        };

        if (!toggleColumns.contains(index.column()))
            return;

        const auto sourceModelIndex = _hierarchyWidget.toSourceModelIndex(index);

        auto& actionsModel = const_cast<QAbstractItemModel&>(_hierarchyWidget.getModel());

        actionsModel.setData(sourceModelIndex, !actionsModel.data(sourceModelIndex, Qt::EditRole).toBool(), Qt::EditRole);
    });

    auto layout = new QVBoxLayout();

    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&_hierarchyWidget);
    layout->addWidget(editSubsetsAction->getOperationsAction().createWidget(this));

    setLayout(layout);

    const auto updateButtons = [this, &plugin, editSubsetsAction]() -> void {
        const auto selectedRows     = plugin.getSelectionModel().selectedRows();
        const auto hasSelection     = !selectedRows.isEmpty();
        const auto multiSelection   = selectedRows.count() >= 2;

        editSubsetsAction->getRemoveSubsetAction().setEnabled(hasSelection);

        auto selectedRowIndex = hasSelection ? selectedRows.first().row() : -1;

        editSubsetsAction->getMoveSubsetToTopAction().setEnabled(!multiSelection && hasSelection && selectedRowIndex > 0);
        editSubsetsAction->getMoveSubsetUpAction().setEnabled(!multiSelection && selectedRowIndex > 0 ? selectedRowIndex > 0 : false);
        editSubsetsAction->getMoveSubsetDownAction().setEnabled(!multiSelection && selectedRowIndex >= 0 ? selectedRowIndex < _hierarchyWidget.getModel().rowCount() - 1 : false);
        editSubsetsAction->getMoveSubsetToBottomAction().setEnabled(!multiSelection && hasSelection && selectedRowIndex < _hierarchyWidget.getModel().rowCount() - 1);
        editSubsetsAction->getSetSelectionAction().setEnabled(!multiSelection && hasSelection);
        editSubsetsAction->getExportClusterAction().setEnabled(!plugin.getSubsetModel().getSubsets().isEmpty());

        // plugin.getChannelHistogramWidget()->update();
    };

    connect(&plugin.getSelectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtons);
    connect(&_hierarchyWidget.getModel(), &QAbstractListModel::rowsRemoved, updateButtons);
    connect(&_hierarchyWidget.getModel(), &QAbstractListModel::rowsMoved, updateButtons);
    connect(&_hierarchyWidget.getModel(), &QAbstractListModel::layoutChanged, updateButtons);

    const auto onRowsInserted = [this, &plugin, updateButtons](const QModelIndex& parent, int first, int last) {
        if (projects().isOpeningProject() || projects().isImportingProject())
            return;

        const auto index = _hierarchyWidget.getModel().index(first, 0);
        
        if (index.isValid())
        plugin.getSelectionModel().select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

        updateButtons();
    };

    connect(&_hierarchyWidget.getModel(), &QAbstractListModel::rowsInserted, this, onRowsInserted);

    connect(&editSubsetsAction->getRemoveSubsetAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.isEmpty())
            return;

        QVector<QPersistentModelIndex> persistentSelectedRows;

        for (auto selectedRow : selectedRows) {
            persistentSelectedRows << selectedRow;
        }

        for (auto persistentSelectedRow : persistentSelectedRows) {
            plugin.getSubsetModel().removeSubset(persistentSelectedRow);
        }
    });

    connect(&editSubsetsAction->getMoveSubsetToTopAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.count() != 1)
            return;

        plugin.getSubsetModel().moveSubset(selectedRows.first(), -1000);
    });

    connect(&editSubsetsAction->getMoveSubsetUpAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.count() != 1)
            return;
    
        plugin.getSubsetModel().moveSubset(selectedRows.first(), -1);
    });

    connect(&editSubsetsAction->getMoveSubsetDownAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.count() != 1)
            return;
        
        plugin.getSubsetModel().moveSubset(selectedRows.first(), 1);
    });

    connect(&editSubsetsAction->getMoveSubsetToBottomAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.count() != 1)
            return;
        
        plugin.getSubsetModel().moveSubset(selectedRows.first(), 1000);
    });

    connect(&editSubsetsAction->getSetSelectionAction(), &TriggerAction::triggered, this, [this, &plugin]() {
        const auto selectedRows = plugin.getSelectionModel().selectedRows();

        if (selectedRows.count() != 1)
            return;

        auto subset = plugin.getSubsetModel().getSubsetFromIndex(selectedRows.first());

        Q_ASSERT(subset != nullptr);

        if (subset == nullptr)
            return;

        plugin.setCurrentSubset(subset);
        plugin.getCurrentDataset()->setSelectionIndices(subset->getIndices());

        mv::events().notifyDatasetDataSelectionChanged(plugin.getCurrentDataset());
    });

    // connect(&editSubsetsAction->getExportClusterAction(), &TriggerAction::triggered, this, [this, &plugin]() {
    //     const auto subsets = plugin.getSubsetModel().getSubsets();

    //     if (subsets.isEmpty())
    //         return;
        
    //     // if (!plugin.getCurrentDataset().isValid())
    //     //     return;

    //     auto clusterDataset = mv::data().createDataset<Clusters>("Cluster", QString("Subsets_%1").arg(_clustersCnt++), plugin.getCurrentDataset());

    //     for (auto subset : subsets) {
    //         Cluster cluster;
    //         cluster.setName(subset->getNameAction().getString());
    //         cluster.setColor(subset->getColorAction().getColor());
    //         cluster.setIndices(subset->getIndices());

    //         clusterDataset->addCluster(cluster);
    //     }
    //     mv::events().notifyDatasetAdded(clusterDataset);
    //     mv::events().notifyDatasetDataChanged(clusterDataset);
    // });

    updateButtons();
}