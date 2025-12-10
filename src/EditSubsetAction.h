#pragma once

#include <actions/GroupsAction.h>

using namespace mv::gui;

/**
 * Edit subset action class
 *
 * Action class for editing an image subset
 *
 * @author Thomas Kroes
 */
class EditSubsetAction : public GroupsAction
{
    Q_OBJECT

public:

    /**
     * Construct with \p parent object and \p title
     * @param parent Pointer to parent object
     * @param title Title
     */
    Q_INVOKABLE EditSubsetAction(QObject* parent, const QString& title);
};

Q_DECLARE_METATYPE(EditSubsetAction)

inline const auto editSubsetActionMetaTypeId = qRegisterMetaType<EditSubsetAction*>("EditSubsetAction");