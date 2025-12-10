#include "SubsetModel.h"
#include "XRFAnalysisPlugin.h"

#include <Application.h>
#include <CoreInterface.h>
#include <DataHierarchyItem.h>
#include <util/Exception.h>
#include <event/Event.h>

#include <QPainter>
#include <QMessageBox>

using namespace mv;
using namespace mv::util;

SubsetModel::HeaderItem::HeaderItem(const ColumHeaderInfo& columHeaderInfo) :
    QStandardItem(),
    _columHeaderInfo(columHeaderInfo)
{
}

QVariant SubsetModel::HeaderItem::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return _columHeaderInfo._display;

        case Qt::EditRole:
            return _columHeaderInfo._edit;

        case Qt::ToolTipRole:
            return _columHeaderInfo._tooltip;

        default:
            break;
    }

    return QVariant();
}

SubsetModel::Item::Item(Subset* subset, bool editable /*= false*/) :
    QStandardItem(),
    QObject(),
    _subset(subset)
{
    Q_ASSERT(_subset != nullptr);

    setEditable(editable);
    setDropEnabled(true);

    connect(&getSubset()->getVisibleAction(), &ToggleAction::toggled, this, [this](bool toggled) -> void {
        emitDataChanged();
    });
}

QPointer<Subset> SubsetModel::Item::getSubset() const
{
    return _subset;
}

QVariant SubsetModel::Item::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role) {
        case Qt::ForegroundRole:
            return QColor(getSubset()->getVisibleAction().isChecked() ? QApplication::palette().color(QPalette::Text) : QApplication::palette().color(QPalette::Disabled, QPalette::Text));

        default:
            break;
    }

    return QStandardItem::data(role);
}

SubsetModel::VisibleItem::VisibleItem(Subset* subset) :
    Item(subset)
{
    setCheckable(true);
    setCheckState(getSubset()->getVisibleAction().isChecked()? Qt::Checked : Qt::Unchecked);
}

QVariant SubsetModel::VisibleItem::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role) {
        case Qt::EditRole:
            return getSubset()->getVisibleAction().isChecked();

        case Qt::ToolTipRole:
            return QString("Subset is: %1").arg(data(Qt::EditRole).toBool() ? "visible" : "hidden");

        case Qt::CheckStateRole:
            return data(Qt::EditRole).toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

        default:
            break;
    }

    return Item::data(role);
}

void SubsetModel::VisibleItem::setData(const QVariant& value, int role /* = Qt::UserRole + 1 */)
{
    switch (role) {
        case Qt::CheckStateRole:
            getSubset()->getVisibleAction().setChecked(value.toBool());

        default:
            Item::setData(value, role);
    }
}

SubsetModel::ColorItem::ColorItem(Subset* subset) :
    Item(subset)
{
    connect(&getSubset()->getColorAction(), &ColorAction::colorChanged, this, [this](const QColor& color) -> void {
        emitDataChanged();
    });
}

QVariant SubsetModel::ColorItem::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role) {
        case Qt::EditRole:
            return getSubset()->getColorAction().getColor();

        case Qt::ToolTipRole:
        {
            const auto color = data(Qt::EditRole).value<QColor>();

            return QString("Color: rgb(%1, %2, %3)").arg(QString::number(color.red()), QString::number(color.green()), QString::number(color.blue()));
        }

        case Qt::DecorationRole:
        {
            const auto color = getSubset()->getColorAction().getColor();

            if (getSubset()->getVisibleAction().isChecked())
                return getColorIcon(color);
            else
                return getColorIcon(QColor::fromHsl(color.hue(), 0, color.lightness()));

        }

        default:
            break;
    }

    return Item::data(role);
}

void SubsetModel::ColorItem::setData(const QVariant& value, int role /* = Qt::UserRole + 1 */)
{
    switch (role) {
        case Qt::EditRole:
            getSubset()->getColorAction().setColor(value.value<QColor>());
            break;

        default:
            Item::setData(value, role);
    }
}

QIcon SubsetModel::ColorItem::getColorIcon(const QColor& color) const
{
    QPixmap pixmap(QSize(13, 13));

    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    const auto radius = 3;

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(color));
    painter.drawRoundedRect(0, 0, pixmap.width(), pixmap.height(), radius, radius);

    return QIcon(pixmap);
}

SubsetModel::NameItem::NameItem(Subset* subset) :
    Item(subset)
{
    setEditable(true);

    connect(&getSubset()->getNameAction(), &StringAction::stringChanged, this, [this](const QString& string) -> void {
        emitDataChanged();
    });
}

QVariant SubsetModel::NameItem::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return getSubset()->getNameAction().getString();

        case Qt::ToolTipRole:
            return QString("Name: %1").arg(data(Qt::DisplayRole).toString());

        default:
            break;
    }

    return Item::data(role);
}

void SubsetModel::NameItem::setData(const QVariant& value, int role /* = Qt::UserRole + 1 */)
{
    switch (role) {
        case Qt::EditRole:
            getSubset()->getNameAction().setString(value.toString());
            break;

        default:
            Item::setData(value, role);
    }
}

QVariant SubsetModel::SubsetSizeItem::data(int role /*= Qt::UserRole + 1*/) const
{
    switch (role) {
        case Qt::EditRole:
            return getSubset()->getSubsetSize();

        case Qt::DisplayRole:
            return QString::number(data(Qt::EditRole).toInt());

        case Qt::ToolTipRole:
            return QString("Subset size: %1").arg(data(Qt::DisplayRole).toString());

        default:
            break;
    }

    return Item::data(role);
}

SubsetModel::Row::Row(Subset* subset)
{
    append(new VisibleItem(subset));
    append(new ColorItem(subset));
    append(new NameItem(subset));
    append(new SubsetSizeItem(subset));
}

QMap<SubsetModel::Column, SubsetModel::ColumHeaderInfo> SubsetModel::columnInfo = QMap<SubsetModel::Column, SubsetModel::ColumHeaderInfo>({
    { SubsetModel::Column::Visible, { "" , "Visible", "Whether the visible or not" } }, 
    { SubsetModel::Column::Color, { "" , "Color", "Subset color" } }, 
    { SubsetModel::Column::Name, { "Name" , "Name", "Name of the subset" } }, 
    { SubsetModel::Column::SubsetSize, { "Size" , "Size", "Size of subset" } }
});

SubsetModel::SubsetModel(XRFAnalysisPlugin* plugin) :
    _XRFAnalysisPlugin(nullptr)
{
    setColumnCount(static_cast<int>(Column::Count));
    _XRFAnalysisPlugin = plugin;

    for (auto column : columnInfo.keys()) {
        setHorizontalHeaderItem(static_cast<int>(column), new HeaderItem(columnInfo[column]));
    }
}

SubsetModel::~SubsetModel()
{

}

Qt::ItemFlags SubsetModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    auto itemFlags = itemFromIndex(index)->flags();

    if (index.column() == static_cast<int>(Column::Visible))
        itemFlags |= Qt::ItemIsUserCheckable;

    return itemFlags;
}

Subset* SubsetModel::getSubsetFromIndex(const QModelIndex& index) const
{
    return static_cast<SubsetModel::Item*>(itemFromIndex(index))->getSubset();
}

SubsetModel::Subsets SubsetModel::getSubsets() const
{
    Subsets subsets;

    for (int idx = 0; idx < rowCount(); idx++)
        subsets << static_cast<Item*>(itemFromIndex(index(idx, 0)))->getSubset();

    return subsets;
}

void SubsetModel::addSubset(Subset* subset) {
    try
    {
        Q_ASSERT(subset != nullptr);

        if (subset == nullptr)
            return;

        insertRow(0, Row(subset));
    }
    catch (std::exception& e)
    {
        exceptionMessageBox("Unable to add subset to the subset model", e);
    }
    catch (...) {
        exceptionMessageBox("Unable to add subset to the subset model");
    }
}

void SubsetModel::removeSubset(const std::uint32_t& row)
{
    try
    {
        removeRow(row, QModelIndex());
    }
    catch (std::exception& e)
    {
        exceptionMessageBox("Unable to remove subset from the subset model", e);
    }
    catch (...) {
        exceptionMessageBox("Unable to remove subset from the subset model");
    }
}

void SubsetModel::removeSubset(const QModelIndex& subsetModelIndex)
{
    removeSubset(subsetModelIndex.row());
}

void SubsetModel::moveSubset(const QModelIndex& subsetModelIndex, const std::int32_t& amount /*= 1*/)
{
    try
    {
        auto row = takeRow(subsetModelIndex.row());

        if (amount < 0)
            insertRow(std::clamp(subsetModelIndex.row() + amount, 0, rowCount() - 1), row);
        else
            insertRow(std::clamp(subsetModelIndex.row() + amount, 0, rowCount()), row);
    }
    catch (std::exception& e)
    {
        exceptionMessageBox("Unable to move subset in the subsets model", e);
    }
    catch (...) {
        exceptionMessageBox("Unable to move subset in the subsets model");
    }
}