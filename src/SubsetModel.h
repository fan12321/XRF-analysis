#pragma once

#include "Subset.h"

// image viewer
#include <QStandardItemModel>

// spectral viewer
#include <QAbstractListModel>
class XRFAnalysisPlugin;

class SubsetModel final : public QStandardItemModel
{
public:
    enum class Column {
        Visible, 
        Color, 
        Name, 
        SubsetSize, 

        Count
    };

    struct ColumHeaderInfo {
        QString     _display;   /** Header string for display role */
        QString     _edit;      /** Header string for edit role */
        QString     _tooltip;   /** Header string for tooltip role */
    };

    static QMap<Column, ColumHeaderInfo> columnInfo;

protected:

    /** Header standard model item class */
    class HeaderItem : public QStandardItem {
    public:
        HeaderItem(const ColumHeaderInfo& columHeaderInfo);
        QVariant data(int role = Qt::UserRole + 1) const override;
    private:
        ColumHeaderInfo     _columHeaderInfo;   /** Column header info */
    };

public:
    class Item: public QStandardItem, public QObject {
    public:
        Item(Subset* layer, bool editable = false);
        QVariant data(int role = Qt::UserRole + 1) const override;
        QPointer<Subset> getSubset() const;

    private:
        QPointer<Subset> _subset;
    };

protected:
    class VisibleItem final : public Item {
    public:
        VisibleItem(Subset* subset);
        QVariant data(int role = Qt::UserRole + 1) const override;
        void setData(const QVariant& value, int role /* = Qt::UserRole + 1 */) override;
    };

    class ColorItem final : public Item {
    public:
        ColorItem(Subset* subset);
        QVariant data(int role = Qt::UserRole + 1) const override;
        void setData(const QVariant& value, int role /* = Qt::UserRole + 1 */) override;

    private:
        QIcon getColorIcon(const QColor& color) const;
    };

    class NameItem final : public Item {
    public:
        NameItem(Subset* subset);
        QVariant data(int role = Qt::UserRole + 1) const override;
        void setData(const QVariant& value, int role /* = Qt::UserRole + 1 */) override;
    };

    class SubsetSizeItem final : public Item {
    public:
        using Item::Item;
        QVariant data(int role = Qt::UserRole + 1) const override;
    };

    class Row final : public QList<QStandardItem*>
    {
    public:

        /**
         * Construct row with \p layer
         * @param layer Pointer to row layer
         */
        Row(Subset* subset);
    };

public:
    using Subsets = QVector<Subset*>;
    SubsetModel(XRFAnalysisPlugin* plutin);
    ~SubsetModel();
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    Subset* getSubsetFromIndex(const QModelIndex& index) const;
    Subsets getSubsets() const;

    void addSubset(Subset*);
    void removeSubset(const std::uint32_t& row);
    void removeSubset(const QModelIndex& layerModelIndex);
    void moveSubset(const QModelIndex& layerModelIndex, const std::int32_t& amount = 1);
private:
    XRFAnalysisPlugin* _XRFAnalysisPlugin;
};
