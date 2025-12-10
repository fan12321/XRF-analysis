#pragma once

#include <actions/TriggerAction.h>
#include <actions/GroupAction.h>

#include <widgets/HierarchyWidget.h>

#include <QRandomGenerator>

class QMenu;
class FunctionWidgetAction;

using namespace mv::gui;

class EditSubsetsAction: public WidgetAction
{
public:
    class Widget : public WidgetActionWidget
    {
    protected:
        Widget(QWidget* parent, EditSubsetsAction* editSubsetsAction);

    private:
        HierarchyWidget     _hierarchyWidget;       /** Subsets widget */
        int                 _clustersCnt {0};
        friend class EditSubsetsAction;
    };

protected:

    /**
     * Get widget representation of the subsets action
     * @param parent Pointer to parent widget
     * @param widgetFlags Widget flags for the configuration of the widget
     */
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
        return new Widget(parent, this);
    };

public:

    Q_INVOKABLE EditSubsetsAction(QObject* parent, const QString& title);

    static QColor getRandomSubsetColor();

public: // Action getters

    FunctionWidgetAction& getFunctionWidgetAction() { return _functionWidgetAction; }
    TriggerAction& getRemoveSubsetAction() { return _removeSubsetAction; }
    TriggerAction& getMoveSubsetToTopAction() { return _moveSubsetToTopAction; }
    TriggerAction& getMoveSubsetUpAction() { return _moveSubsetUpAction; }
    TriggerAction& getMoveSubsetDownAction() { return _moveSubsetDownAction; }
    TriggerAction& getMoveSubsetToBottomAction() { return _moveSubsetToBottomAction; }
    TriggerAction& getSetSelectionAction() { return _setSelectionAction; }
    TriggerAction& getExportClusterAction() { return _exportClusterAction; }
    GroupAction& getOperationsAction() { return _operationsAction; }

private:
    FunctionWidgetAction&     _functionWidgetAction;            /** Reference to settings action */
    TriggerAction       _removeSubsetAction;         /** Remove subset action */
    TriggerAction       _moveSubsetToTopAction;      /** Move subset to top action */
    TriggerAction       _moveSubsetUpAction;         /** Move subset up action */
    TriggerAction       _moveSubsetDownAction;       /** Move subset down action */
    TriggerAction       _moveSubsetToBottomAction;   /** Move subset to bottom action */
    TriggerAction       _setSelectionAction;        /** Hierarchy widget action */
    TriggerAction       _exportClusterAction;        /** Export cluster action */
    GroupAction         _operationsAction;          /** Action for grouping various operations */

    static QRandomGenerator     rng;    /** Random number generator for pseudo-random subset colors */
};
