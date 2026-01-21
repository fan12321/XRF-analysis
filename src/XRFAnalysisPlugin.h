#pragma once

#include <ViewPlugin.h>

#include <Dataset.h>
#include <PointData/PointData.h>
#include <widgets/DropWidget.h>
#include <actions/HorizontalToolbarAction.h>
#include <actions/TriggerAction.h>

#include <QWidget>

#include "StatisticsAction.h"
#include "FunctionWidgetAction.h"
#include "SubsetModel.h"

/** All plugin related classes are in the ManiVault plugin namespace */
using namespace mv::plugin;

/** Drop widget used in this plugin is located in the ManiVault gui namespace */
using namespace mv::gui;

/** Dataset reference used in this plugin is located in the ManiVault util namespace */
using namespace mv::util;

class ChartWidget;

/**
 * Example view JS plugin class
 * 
 * This plugin showcases how a JavaScript-based visualization can be included in ManiVault 
 * Here, we use a D3 library, but other libraries like Vega-Lite follow the same pattern
 * 
 * This project:
 *  - Sets up a WebWidget, which displays an HTML webpage
 *  - Connects selections made a D3 plot with ManiVault
 * 
 * This projects does not implement selections from ManiVault to the D3 plot,
 * but such implementation follows the same form as the data-values communication
 * between cpp and JavaScript that is used here.
 *
 * @authors J. Thijssen & T. Kroes & A. Vieth
 */
class XRFAnalysisPlugin : public ViewPlugin
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    XRFAnalysisPlugin(const PluginFactory* factory);

    /** Destructor */
    ~XRFAnalysisPlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

    /** Store a private reference to the data set that should be displayed */
    void loadData(const mv::Datasets& datasets) override;

    mv::Dataset<Points> getCurrentDataset() { return _currentDataSet; };

    SubsetModel& getSubsetModel() { return *_subsetModel; };
    FunctionWidgetAction& getFunctionWidget() { return _functionWidgetAction; };
    QItemSelectionModel& getSelectionModel() { return _selectionModel; };

    void setCurrentSubset(Subset* subset) {
        _currentSubset = subset;
        _lockSubset = true;
    };
    void addSubset();

public slots:
    /** Converts ManiVault's point data to a json-like data structure that Qt can pass to the JS code */
    void convertDataAndUpdateChart();
    void updateThreshold();

private:
    /** Published selections received from the JS side to ManiVault's core */
    void publishSelection(const std::vector<unsigned int>& selectedIDs);
    void calculateFocusingElementDetail(const QVariantMap& data);
    void splitGaussians(const QVariantMap& data);
    void compareSubsets(const QVariantMap& data);

    void toQuantile(std::vector<float>&, int);

    QVariantMap createHierarchyMap(Subset* node);

    QString getCurrentDataSetID() const;


private:
    ChartWidget*            _chartWidget;       // WebWidget that sets up the HTML page
    DropWidget*             _dropWidget;        // Widget for drag and drop behavior
    mv::Dataset<Points>     _currentDataSet;    // Reference to currently shown data set

    int                     _numOfChannels {1};
    std::vector<float>      _channelMinima;
    std::vector<float>      _channelMaxima;
    std::vector<float>      _histogramsFull;
    std::vector<std::vector<int>>
                            _subsetIndices;
    std::vector<std::vector<float>>
                            _subsetValues;

    std::vector<float>      _cuts;


    std::vector<std::vector<float>>
                            _quantiles;
    int _quantileGroups {99};
    StatisticsAction        _statisticsAction;
    TriggerAction           _addSubsetAction;
    HorizontalToolbarAction _primaryToolbarAction;

    std::map<QString, int>  _channel_ID_map;
    SubsetModel*            _subsetModel;
    Subset*                 _currentSubset;
    bool                    _lockSubset {false};
    QItemSelectionModel     _selectionModel;
    FunctionWidgetAction        _functionWidgetAction;

    int subsetUniqueID {0};
};

/**
 * Example view plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class XRFAnalysisPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.XRFAnalysisPlugin"
                      FILE  "PluginInfo.json")

public:

    /** Default constructor */
    XRFAnalysisPluginFactory();

    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
