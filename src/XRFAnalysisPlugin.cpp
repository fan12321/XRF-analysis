#include "XRFAnalysisPlugin.h"

#include "ChartWidget.h"
#include "GMM.h"

#include <DatasetsMimeData.h>

#include <vector>
#include <random>
#include <limits>
#include <algorithm>

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeData>
#include <QDebug>
#include <QtConcurrent>
#include <QFuture>

#include <omp.h>

Q_PLUGIN_METADATA(IID "studio.manivault.XRFAnalysisPlugin")

using namespace mv;

const int HISTOGRAM_RESOLUTION = 9;

std::vector<int> histogramCalculation(std::vector<float> &arr, float min, float max, int resolution = HISTOGRAM_RESOLUTION, int bucketSize = -1)
{
    std::vector<int> histogram(resolution);
    if (min == max)
        return histogram;
    for (float val : arr)
    {
        int bucketId = static_cast<int>((resolution - 1) * (val - min) / (max - min));
        if (bucketSize > 0) {
            while (histogram[bucketId] == bucketSize) {
                bucketId += 1;
            }
        }
        histogram[bucketId] += 1;
    }

    return histogram;
};

int numberOfPeaksCalculation(std::vector<int> &histogram)
{
    int resolution = histogram.size();
    if (resolution == 0) return 0;

    int peakValue = histogram[0];
    for (auto val: histogram) {
        peakValue = (peakValue > val)? val: peakValue;
    }

    int cnt = 0;
    if (histogram[0] > histogram[1])
        cnt += 1;
    for (int i = 1; i < resolution - 1; i++)
    {
        if (
            histogram[i] >= histogram[i - 1] &&
            histogram[i] > histogram[i + 1] &&
            histogram[i] > peakValue * 0.05)
        {
            cnt += 1;
        }
    }
    if (histogram[resolution-1] > histogram[resolution-2])
        cnt += 1;

    return cnt;
};

float meanCalculation(std::vector<float>& arr) {
    if (arr.empty())
        return 0.0f;

    auto count = static_cast<float>(arr.size());
    return std::reduce(arr.begin(), arr.end()) / count;
};

float varCalculation(std::vector<float>& arr) {
    if (arr.empty())
        return 0.0f;

    int size = arr.size();
    float mean = std::reduce(arr.begin(), arr.end()) / static_cast<float>(size);
    
    std::vector<float> diff(size);
    #pragma omp parallel for
    for (int i=0; i<size; i++) {
        diff[i] = (arr[i] - mean) * (arr[i] - mean);
    }
    return (size>1)? std::reduce(diff.begin(), diff.end()) / (size-1) : 0.0;
};

float sdCalculation(std::vector<float>& arr) {
    return std::sqrt(varCalculation(arr));
};


XRFAnalysisPlugin::XRFAnalysisPlugin(const PluginFactory *factory) : ViewPlugin(factory),
                                                                     _subsetModel(new SubsetModel(this)),
                                                                     _selectionModel(_subsetModel),
                                                                     _currentSubset(nullptr), 
                                                                     _chartWidget(nullptr),
                                                                     _dropWidget(nullptr),
                                                                     _currentDataSet(nullptr),
                                                                     _clusterDataset(nullptr), 
                                                                     _statisticsAction(this, "Statistics"),
                                                                     _addSubsetAction(this, "Set ROI"),
                                                                     _linkChannelViewAction(this, "Link channel view"), 
                                                                     _primaryToolbarAction(this, "Primary toolbar"),
                                                                     _functionWidgetAction(this, "Functions")
{
    getLearningCenterAction().addVideos(QStringList({"Practitioner", "Developer"}));

    _primaryToolbarAction.setDefaultWidgetFlags(GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_statisticsAction);
    _primaryToolbarAction.addAction(&_addSubsetAction);
    _primaryToolbarAction.addAction(&_linkChannelViewAction);
    _linkChannelViewAction.setIconByName("window-maximize");
    _linkChannelViewAction.setText("Link channel view");
}

void XRFAnalysisPlugin::init()
{
    lastUpdate = std::chrono::system_clock::now();
    getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Create layout
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);

    layout->setSpacing(0);
    layout->addWidget(_primaryToolbarAction.createWidget(&getWidget()));

    // Create chart widget and set html contents of webpage
    _chartWidget = new ChartWidget(this);
    _chartWidget->setPage(":xrf_chart/xrf.html", "qrc:/xrf_chart/");

    // Add widget to layout
    layout->addWidget(_chartWidget, 100);

    // Apply the layout
    getWidget().setLayout(layout);

    addDockingAction(&_functionWidgetAction, nullptr, DockAreaFlag::Left, true, AutoHideLocation::Right, QSize(300, 300));

    // Instantiate new drop widget: See ExampleViewPlugin for details
    _dropWidget = new DropWidget(_chartWidget);
    _dropWidget->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the XRFAnalysisPlugin in this view"));

    _dropWidget->initialize([this](const QMimeData *mimeData) -> DropWidget::DropRegions
                            {

        // A drop widget can contain zero or more drop regions
        DropWidget::DropRegions dropRegions;

        const auto datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);


        if (datasetsMimeData == nullptr)
            return dropRegions;

        if (datasetsMimeData->getDatasets().count() > 1)
            return dropRegions;

        const auto dataset = datasetsMimeData->getDatasets().first();
        const auto datasetGuiName = dataset->text();
        const auto datasetId = dataset->getId();
        const auto dataType = dataset->getDataType();
        const auto dataTypes = DataTypes({ PointType, ClusterType });

        if (dataTypes.contains(dataType)) {

            if (datasetId == getCurrentDataSetID()) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else if (dataType == ClusterType) {
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);

                dropRegions << new DropWidget::DropRegion(this, "Cluster", QString("Visualize %1 as parallel coordinates").arg(datasetGuiName), "map-marker-alt", true, [this, candidateDataset]() {
                    loadData({ candidateDataset });
                    _dropWidget->setShowDropIndicator(false);
                    });
            }
            else if (dataType == PointType) {
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);

                dropRegions << new DropWidget::DropRegion(this, "Points", QString("Visualize %1 as parallel coordinates").arg(datasetGuiName), "map-marker-alt", true, [this, candidateDataset]() {
                    loadData({ candidateDataset });
                    _dropWidget->setShowDropIndicator(false);
                    });

            }
        }
        else {
            dropRegions << new DropWidget::DropRegion(this, "Incompatible data", "This type of data is not supported", "exclamation-circle", false);
        }

        return dropRegions; });

    const auto scanChannelViews = [this]() -> void
    {
        auto viewFactory = mv::plugins().getPluginFactory("Channel View JS");
        QStringList names;
        QString currentScatterplot = _linkChannelViewAction.getCurrentText();
        names << "Empty";
        if (viewFactory)
        {
            for (auto plugin : mv::plugins().getPluginsByFactory(viewFactory))
            {
                names << plugin->getGuiName();
            }
        }
        if (currentScatterplot == "") currentScatterplot = "Empty";
        _linkChannelViewAction.initialize(names, currentScatterplot);
    };

    connect(&mv::plugins(), &AbstractPluginManager::pluginAdded, this, scanChannelViews);
    connect(&_linkChannelViewAction, &OptionAction::currentIndexChanged, this, [this]() {
        auto viewFactory = mv::plugins().getPluginFactory("Channel View JS");
        if (viewFactory) {
            for (auto plugin : mv::plugins().getPluginsByFactory(viewFactory)) {
                if (plugin->getGuiName() == _linkChannelViewAction.getCurrentText()) {
                    _linkedChannelView = plugin;
                    return;
                }
            }
        }
        _linkedChannelView = nullptr;
    });
    scanChannelViews();

    // update data when data set changed
    // connect(&_currentDataSet, &Dataset<Points>::dataChanged, this, [this]() {
    //     convertDataAndUpdateChart();
    //     if (_linkedChannelView) {
    //         auto dataPickerAction = dynamic_cast<DatasetPickerAction*>(_linkedChannelView->findChildByPath("Current dataset"));
    //         if (_currentDataSet.isValid()) {
    //             dataPickerAction->setCurrentDataset(_currentDataSet);
    //         }
    //     }
    // });
    connect(&_currentDataSet, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        // if (_clusterDataset.isValid()) {
            //     mv::data().removeDataset(_clusterDataset);
            //     events().notifyDatasetDataChanged(_currentDataSet);
            // }
        convertDataAndUpdateChart();
    });


    connect(&_statisticsAction.getStatisticsAction(), &OptionAction::currentIndexChanged, this, [this]() {
        if (_currentSubset) {
            _currentDataSet->setSelectionIndices(_currentSubset->getIndices());
            events().notifyDatasetDataSelectionChanged(_currentDataSet);
            _lockSubset = true;
        }
        else convertDataAndUpdateChart();
    });

    connect(&_statisticsAction.getRelativeMeanTreeAction(), &DecimalAction::valueChanged, this, [this]() {
        auto root = _currentSubset;
        if (root) {
            root = _currentSubset->getRoot();
        }
        QVariantMap rootData = createHierarchyMap(root);
        emit _chartWidget->getCommunicationObject().qt_js_setTreeData(rootData);
    });
    connect(&_statisticsAction.getMeanValueAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getRelativeMeanValueAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getVarianceAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getMADAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getNumberOfPeaksAction(), &IntegralAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getLogLikelihoodAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);

    connect(&_addSubsetAction, &TriggerAction::triggered, this, [this]() {
        addSubset();
        _currentDataSet->setSelectionIndices(_currentSubset->getIndices());
        events().notifyDatasetDataSelectionChanged(_currentDataSet);
        _lockSubset = true;
    });

    // Update the selection (coming from PCP) in core
    // connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &XRFAnalysisPlugin::publishSelection);

    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passFocusingElementToCore, this, &XRFAnalysisPlugin::calculateFocusingElementDetail);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passDblClickSplit, this, &XRFAnalysisPlugin::splitGaussians);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passNodeId, this, [this](const QVariantMap& data) {
        int subsetId = data["id"].toInt();
        for (auto subset: _subsetModel->getSubsets()) {
            if (subset->getId() == subsetId) {
                _currentDataSet->setSelectionIndices(subset->getIndices());
                events().notifyDatasetDataSelectionChanged(_currentDataSet);
                _lockSubset = true;
                _currentSubset = subset;
                return;
            }
        }
    });
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSplitCuts, this, &XRFAnalysisPlugin::splitCuts);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passPreviewSplits, this, &XRFAnalysisPlugin::previewSplits);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passNoSplitsSignal, this, [this]() {
        if (_clusterDataset.isValid()) {
            // mv::data().datasetAboutToBeRemoved(_clusterDataset);

            mv::data().removeDataset(_clusterDataset);
            _clusterDataset = mv::Dataset<Clusters>(nullptr);
        }
    });
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passParentNodeId, this, &XRFAnalysisPlugin::compareSubsets);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passCompareClustersSignal, this, &XRFAnalysisPlugin::compareClusters);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passMatchingElements, this, [this](const QVariantMap& data) {
        if (_currentSubset == nullptr) return;
        auto root = _currentSubset->getRoot();
        root->setMatchingElements(data["elements"]);
    });
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passElementToImageViewer, this, [this](const QVariantMap& element) {
        // int channelId = _channel_ID_map[element["element"].toString()];
        // qDebug() << channelId;
        // auto imageViewerFactory = mv::plugins().getPluginFactory("Image Viewer");
        // if (imageViewerFactory) {
        //     for (auto plugin: mv::plugins().getPluginsByFactory(imageViewerFactory)) {
        //         continue;
        //     }
        // }
    });
    
    // addNotification(getExampleNotificationMessage());
}

void XRFAnalysisPlugin::loadData(const mv::Datasets &datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    if (datasets.first()->getDataType() == ClusterType) {
        if (!_currentDataSet.isValid()) return;
        auto inputClusterName = datasets.first()->getGuiName();
        auto subsets = _subsetModel->getSubsets();
        for (auto subset: subsets) {
            if (subset->getName() == inputClusterName) {
                setCurrentSubset(subset);
                _currentDataSet->setSelectionIndices(subset->getIndices());
                mv::events().notifyDatasetDataSelectionChanged(_currentDataSet);
                return;
            }
        }
    }

    qDebug() << "XRFAnalysisPlugin::loadData: Load data set from ManiVault core";

    // Load the first dataset, changes to _currentDataSet are connected with convertDataAndUpdateChart
    _currentDataSet = datasets.first();
    _numOfChannels = _currentDataSet->getNumDimensions();
    _quantiles = std::vector<std::vector<float>>(_numOfChannels);
    _channelMaxima = std::vector<float>(_numOfChannels);
    _channelMinima = std::vector<float>(_numOfChannels);
    _channelMean = std::vector<float>(_numOfChannels);
    auto channelNames = _currentDataSet->getDimensionNames();
    int size = _currentDataSet->getNumPoints();
    int stripe = size / (_quantileGroups+1);

    std::vector<QFuture<void>> futureVector(_numOfChannels);
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        auto future = QtConcurrent::run([this, channelId, stripe, size]() {
            std::vector<float> quantile(_quantileGroups);
            std::vector<float> data;
            _currentDataSet->extractDataForDimension(data, channelId);
            std::sort(data.begin(), data.end());
    
            for (int i=0; i<_quantileGroups; i++) {
                quantile[i] = data[(i+1)*stripe];
            }
            _quantiles[channelId].assign(quantile.begin(), quantile.end());

            _channelMaxima[channelId] = data[size-1];
            _channelMinima[channelId] = data[0];
            _channelMean[channelId] = (size==0)? 0.0 :  std::reduce(data.begin(), data.end()) / size;
        });
        QString channelName = channelNames[channelId].first(3);
        _channel_ID_map.insert({channelName, channelId});
        futureVector[channelId] = future;
    }
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        futureVector[channelId].waitForFinished();
    }

    QVariantList channels;
    for (auto channel: channelNames) channels.append(channel.first(3));
    emit _chartWidget->getCommunicationObject().qt_js_setChannelInfo(channels);
    events().notifyDatasetDataChanged(_currentDataSet);
}

void XRFAnalysisPlugin::updateThreshold()
{
    if (!_currentDataSet.isValid())
        return;

    float threshold;
    int type = _statisticsAction.getStatisticsAction().getCurrentIndex();
    if (type == 0)
    {
        threshold = _statisticsAction.getMeanValueAction().getValue();
    }
    else if (type == 1)
    {
        threshold = _statisticsAction.getRelativeMeanValueAction().getValue();
    }
    else if (type == 2)
    {
        threshold = _statisticsAction.getVarianceAction().getValue();
    }
    else if (type == 3)
    {
        threshold = _statisticsAction.getMADAction().getValue();
    }
    else if (type == 4)
    {
        threshold = _statisticsAction.getNumberOfPeaksAction().getValue();
    }
    else if (type == 5) {
        threshold = _statisticsAction.getLogLikelihoodAction().getValue();
    }
    QVariant statisticsType(type);
    QVariant thresholdVariant(threshold);
    QVariantList toJs;
    toJs.append(statisticsType);
    toJs.append(thresholdVariant);
    emit _chartWidget->getCommunicationObject().qt_js_setThreshold(toJs);
}

void XRFAnalysisPlugin::addSubset()
{
    if (!_currentDataSet.isValid())
        return;

    auto subset = new Subset(&_functionWidgetAction.getEditSubsetsAction(), "Subset setting", subsetUniqueID);
    subset->initialize(this);
    // subset->setVisibility(true);
    auto rng = QRandomGenerator::global();
    subset->setColor(QColor::fromHsl(rng->bounded(360), rng->bounded(150, 255), rng->bounded(100, 200)));
    subset->setName("ROI_" + QString::number(subsetUniqueID));
    subsetUniqueID++;
    subset->setIndices(_currentDataSet->getSelectionIndices());
    _subsetModel->addSubset(subset);
    _currentSubset = subset;
    // convertDataAndUpdateChart();
}

void XRFAnalysisPlugin::normalize(std::vector<float>& arr, float min, float max) {
    #pragma omp parallel for
    for (int i=0; i<arr.size(); i++) {
        arr[i] = (arr[i] - min) / (max - min);
    }
}

void XRFAnalysisPlugin::convertDataAndUpdateChart()
{
    if (!_lockSubset) _currentSubset = nullptr;
    _lockSubset = false;

    if (!_currentDataSet.isValid())
        return;


    // convert data from ManiVault PointData to a JSON structure
    QVariantList subsets;
    QVariantMap subset;

    subset.clear();
    subset["subset"] = "Selection";
    subset["id"] = -1;
    QVariantList values;
    auto selectionIndices = _currentDataSet->getSelectionIndices();
    int selectionSize = _currentDataSet->getSelectionSize();

    auto statisticsType = _statisticsAction.getStatisticsAction().getCurrentIndex();
    for (int channelId = 0; channelId < _numOfChannels; channelId++)
    {
        QVariantMap elementValue;
        elementValue["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);
        std::vector<float> data;
        data.resize(selectionSize);
        _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, selectionIndices);


        if (statisticsType == 0)
        {
            float avg = meanCalculation(data);
            avg = (avg - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
            
            if (_channelMaxima[channelId] == _channelMinima[channelId]) avg = 0.0;
            elementValue["value"] = avg;
        }
        else if (statisticsType == 1) {
            float avg = meanCalculation(data);
            float avgAll = _channelMean[channelId];

            avg = (avg - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);

            avgAll = (avgAll - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
            
            if (_channelMaxima[channelId] == _channelMinima[channelId]) avg = 0.0;
            elementValue["value"] = avg - avgAll;
        }
        else if (statisticsType == 2) {
            // variance
            normalize(
                data, 
                _channelMinima[channelId], 
                _channelMaxima[channelId]
            );
            float avg = meanCalculation(data);

            #pragma omp parallel for
            for (int i=0; i<selectionSize; i++) {
                data[i] = (data[i] - avg) * (data[i] - avg);
            }

            elementValue["value"] = (selectionSize > 1)? std::reduce(data.begin(), data.end()) / (selectionSize-1) : 0.0;
        }
        else if (statisticsType == 3) {
            // mad
            if (selectionSize == 0) {
                elementValue["value"] = 0.0;
                continue;
            }
            normalize(
                data, 
                _channelMinima[channelId], 
                _channelMaxima[channelId]
            );
            std::sort(data.begin(), data.end());
            float median = data[selectionSize>>1];

            #pragma omp parallel for
            for (int i=0; i<selectionSize; i++) {
                float diff = data[i] - median;
                data[i] = (diff < 0.0)? -diff : diff;
            }

            std::sort(data.begin(), data.end());
            elementValue["value"] = data[selectionSize>>1];
        }
        else if (statisticsType == 4)
        {
            auto histogram = histogramCalculation(
                data, 
                _channelMinima[channelId], 
                _channelMaxima[channelId], 
                HISTOGRAM_RESOLUTION, 
                -1
            );
            int peaksCount = numberOfPeaksCalculation(histogram);

            elementValue["value"] = peaksCount;
        }
        else if (statisticsType == 5) {
            GMM gmm(data);
            gmm.gmm(1);
            elementValue["value"] = -gmm.getLogLikelihood(1) / data.size();
        }

        values.append(elementValue);
    }
    subset["values"] = values;
    subsets.append(subset);


    auto root = _currentSubset;
    if (root) {
        root = _currentSubset->getRoot();
    }
    QVariantMap rootData = createHierarchyMap(root);
    emit _chartWidget->getCommunicationObject().qt_js_setFocusNodeId(_currentSubset? _currentSubset->getId() : QVariant());
    emit _chartWidget->getCommunicationObject().qt_js_setTreeData(rootData);
    emit _chartWidget->getCommunicationObject().qt_js_setMatchingElements(_currentSubset? _currentSubset->getRoot()->getMatchingElements() : QVariant());
    
    updateThreshold();
    emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(subsets);
}

void XRFAnalysisPlugin::compareClusters() {
    if (!_clusterDataset.isValid()) return;
    int subsetSize = _currentDataSet->getSelectionSize();
    int numberOfSplits = _clusterDataset->getClusters().size();
    
    QVariantList splits;
    
    std::vector<float> varList(_numOfChannels);

    // std::vector<QFuture<void>> futureVector(_numOfChannels);
    std::vector<QVariantMap> resultVector(_numOfChannels);
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        // auto future = QtConcurrent::run([this, channelId, numberOfSplits, &resultVector, &varList, &varCalculation]() {
            QVariantMap split;
            split["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);
            float min = _channelMinima[channelId];
            float max = _channelMaxima[channelId];
    
            QVariantList values;
            std::vector<float> avgList;
            
            for (int childId=0; childId<numberOfSplits; childId++) {
                auto cluster = _clusterDataset->getClusters()[childId];
                auto childIndices = cluster.getIndices();
                int splitSize = childIndices.size();
                QVariantMap splitValue;
                splitValue["split"] = cluster.getName();
                std::vector<float> data(splitSize);
                _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, childIndices);

                // box plot statistics
                std::sort(data.begin(), data.end());
                float q0 = (splitSize==0)? 0.0 : data[0];
                float q1 = (splitSize==0)? 0.0 : data[splitSize / 4];
                float q2 = (splitSize==0)? 0.0 : data[splitSize / 2];
                float q3 = (splitSize==0)? 0.0 : data[(splitSize*3) / 4];
                float q4 = (splitSize==0)? 0.0 : data[splitSize-1];
    
                float avg = meanCalculation(data);
                float range = max - min;
                avg = (avg - min) / (max - min);
                if (max == min) avg = 0.0;
                splitValue["value"] = avg;
                splitValue["q0"] = (q0 - min) / range;
                splitValue["q1"] = (q1 - min) / range;
                splitValue["q2"] = (q2 - min) / range;
                splitValue["q3"] = (q3 - min) / range;
                splitValue["q4"] = (q4 - min) / range;
                avgList.push_back(avg);
                values.append(splitValue);
            }
            split["values"] = values;
            varList[channelId] = varCalculation(avgList);
            resultVector[channelId] = split;
        // });
        // futureVector[channelId] = future;
    }
    // for (int channelId = 0; channelId < _numOfChannels; channelId++) {
    //     futureVector[channelId].waitForFinished();
    // }
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        splits.append(resultVector[channelId]);
    }

    std::vector<int> sortedIndices(_numOfChannels);
    std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
    std::sort(sortedIndices.begin(), sortedIndices.end(), [varList](int i, int j) {
        return varList[i] > varList[j];
    });

    QVariantList splitsSorted;
    int cnt = 0;
    for (int idx: sortedIndices) {
        if (cnt++ > 10) break;
        splitsSorted.append(splits[idx]);
    }

    // if (_linkedChannelView) {
    //     auto dataPickerAction = dynamic_cast<DatasetPickerAction*>(_linkedChannelView->findChildByPath("Cluster dataset"));
    //     dataPickerAction->setCurrentDataset(_clusterDataset);
    // }

    emit _chartWidget->getCommunicationObject().qt_js_setPreviewSplits(splitsSorted);
}

void XRFAnalysisPlugin::compareSubsets(const QVariantMap& data) {

    int parentId = data["id"].toInt();    
    Subset* parentSubset;
    for (auto subsetIt: _subsetModel->getSubsets()) {
        if (subsetIt->getId() == parentId) {
            parentSubset = subsetIt;
            break;
        }
    }
    int subsetSize = parentSubset->getIndices().size();
    int numberOfSplits = parentSubset->numberOfChildren();
    
    QVariantList splits;
    
    std::vector<float> varList(_numOfChannels);
    

    std::vector<QFuture<void>> futureVector(_numOfChannels);
    std::vector<QVariantMap> resultVector(_numOfChannels);
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        auto future = QtConcurrent::run([this, channelId, numberOfSplits, parentSubset, &resultVector, &varList]() {
            QVariantMap split;
            split["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);
            float min = _channelMinima[channelId];
            float max = _channelMaxima[channelId];
    
            QVariantList values;
            std::vector<float> avgList;
            
            for (int childId=0; childId<numberOfSplits; childId++) {
                Subset* currSubset = parentSubset->getChildren()[childId];
                auto childIndices = currSubset->getIndices();
                int splitSize = childIndices.size();
                QVariantMap splitValue;
                splitValue["split"] = currSubset->getName();
                std::vector<float> data;
                data.resize(splitSize);
                _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, childIndices);

                // box plot statistics
                std::sort(data.begin(), data.end());
                float q0 = (splitSize==0)? 0.0 : data[0];
                float q1 = (splitSize==0)? 0.0 : data[splitSize / 4];
                float q2 = (splitSize==0)? 0.0 : data[splitSize / 2];
                float q3 = (splitSize==0)? 0.0 : data[(splitSize*3) / 4];
                float q4 = (splitSize==0)? 0.0 : data[splitSize-1];
    
                float avg = meanCalculation(data);
                float range = max - min;
                avg = (avg - min) / (max - min);
                if (max == min) avg = 0.0;
                splitValue["value"] = avg;
                splitValue["q0"] = (q0 - min) / range;
                splitValue["q1"] = (q1 - min) / range;
                splitValue["q2"] = (q2 - min) / range;
                splitValue["q3"] = (q3 - min) / range;
                splitValue["q4"] = (q4 - min) / range;
                avgList.push_back(avg);
                values.append(splitValue);
            }
            split["values"] = values;
            varList[channelId] = varCalculation(avgList);
            resultVector[channelId] = split;
        });
        futureVector[channelId] = future;
    }
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        futureVector[channelId].waitForFinished();
    }
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        splits.append(resultVector[channelId]);
    }

    std::vector<int> sortedIndices(_numOfChannels);
    std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
    std::sort(sortedIndices.begin(), sortedIndices.end(), [varList](int i, int j) {
        return varList[i] > varList[j];
    });

    QVariantList splitsSorted;
    int cnt = 0;
    for (int idx: sortedIndices) {
        if (cnt++ > 10) break;
        splitsSorted.append(splits[idx]);
    }

    emit _chartWidget->getCommunicationObject().qt_js_setSplits(splitsSorted);
}

void XRFAnalysisPlugin::previewSplits(const QVariantMap& cutData) {
    auto splitElementName = cutData["element"].toString();
    auto cuts = cutData["cuts"].toList();
    int channelId = _channel_ID_map[splitElementName];

    if (cuts.count() == 0) {
        if (!_clusterDataset.isValid()) {
            auto clusterName = (_currentSubset == nullptr)? QString("ROI_" + QString::number(subsetUniqueID)) : _currentSubset->getName();
            _clusterDataset = mv::data().createDataset<Clusters>("Cluster", clusterName, _currentDataSet);
        }
        else {
            _clusterDataset->getClusters() = QVector<Cluster>();
            events().notifyDatasetDataChanged(_clusterDataset);
        }
        return;
    }

    _cuts.clear();
    for (auto it: cuts) {
        float cut = it.toFloat();
        cut = _channelMinima[channelId] + cut * (_channelMaxima[channelId] - _channelMinima[channelId]);

        _cuts.push_back(cut);
    }
    int numberOfSplits = _cuts.size() + 1;
    
    QVariantList splits;
    QVariantMap split;
    

    auto subsetIndices = _currentDataSet->getSelectionIndices();
    int subsetSize = subsetIndices.size();

    std::vector<float> dataFull;
    _currentDataSet->extractDataForDimension(dataFull, channelId);

    std::vector<std::vector<uint32_t>> splitIndicesVector(numberOfSplits);
    for (int splitId = 0; splitId < numberOfSplits; splitId++) {
        splitIndicesVector[splitId].reserve(subsetSize);
    }
    for (auto index: subsetIndices) {
        float val = dataFull[index];
        for (int splitId=0; splitId<numberOfSplits-1; splitId++) {
            if (val <= _cuts[splitId]) {
                splitIndicesVector[splitId].push_back(index);
                break;
            }
            if (val > _cuts[numberOfSplits-2]) {
                splitIndicesVector[numberOfSplits-1].push_back(index);
                break;
            }
        }
    }

    if (!_clusterDataset.isValid()) {
        auto clusterName = (_currentSubset == nullptr)? QString("ROI_" + QString::number(subsetUniqueID)) : _currentSubset->getName();
        _clusterDataset = mv::data().createDataset<Clusters>("Cluster", clusterName, _currentDataSet);
    }
    
    int startingId = subsetUniqueID;
    _clusterDataset->getClusters() = QVector<Cluster>();
    for (int splitId = 0; splitId < numberOfSplits; splitId++) {
        Cluster cluster;
        cluster.setName(splitElementName + QString::number(startingId+splitId));
        cluster.setColor(QColor(palette[splitId]));
        cluster.setIndices(splitIndicesVector[splitId]);

        _clusterDataset->addCluster(cluster);
    }
    bool mouseRelease = cutData["mouseRelease"].toBool();
    auto now = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
    // if (dur > 150 || mouseRelease) {
        events().notifyDatasetDataChanged(_clusterDataset);
        lastUpdate = std::chrono::system_clock::now();
    // }
}

void XRFAnalysisPlugin::splitCuts(const QVariantMap& cutData) {
    auto splitElementName = cutData["element"].toString();
    auto cuts = cutData["cuts"].toList();
    int channelId = _channel_ID_map[splitElementName];

    if (cuts.count() == 0) return;

    _cuts.clear();
    for (auto it: cuts) {
        float cut = it.toFloat();
        cut = _channelMinima[channelId] + cut * (_channelMaxima[channelId] - _channelMinima[channelId]);

        _cuts.push_back(cut);
    }
    int numberOfSplits = _cuts.size() + 1;
    
    QVariantList splits;
    QVariantMap split;
    

    auto subsetIndices = _currentDataSet->getSelectionIndices();
    int subsetSize = subsetIndices.size();

    std::vector<float> dataFull;
    _currentDataSet->extractDataForDimension(dataFull, channelId);

    std::vector<std::vector<uint32_t>> splitIndicesVector(numberOfSplits);
    for (int splitId = 0; splitId < numberOfSplits; splitId++) {
        splitIndicesVector[splitId].reserve(subsetSize);
    }
    for (auto index: subsetIndices) {
        float val = dataFull[index];
        for (int splitId=0; splitId<numberOfSplits-1; splitId++) {
            if (val <= _cuts[splitId]) {
                splitIndicesVector[splitId].push_back(index);
                break;
            }
            if (val > _cuts[numberOfSplits-2]) {
                splitIndicesVector[numberOfSplits-1].push_back(index);
                break;
            }
        }
    }

    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        split.clear();
        split["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);

        float min = std::numeric_limits<float>::infinity();
        float max = -std::numeric_limits<float>::infinity();
        std::vector<float> dataSubset(subsetSize);
        _currentDataSet->populateDataForDimensions(dataSubset, std::vector<int>{channelId}, subsetIndices);
        for (auto val: dataSubset) {
            min = (min > val) ? val : min;
            max = (max < val) ? val : max;
        }
        QVariantList values;
        for (int splitId=0; splitId<numberOfSplits; splitId++) {
            const auto& splitIndices = splitIndicesVector[splitId];
            int splitSize = splitIndices.size();
            QVariantMap splitValue;
            splitValue["split"] = splitElementName + QString::number(splitId);
            std::vector<float> data;
            data.resize(splitSize);
            _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, splitIndices);

            float avg = meanCalculation(data);
            avg = (avg - min) / (max - min);
            if (max == min) avg = 0.0;
            splitValue["value"] = avg;

            values.append(splitValue);
        }
        split["values"] = values;
        splits.append(split);
    }

    if (_currentSubset) {
        for (auto child: _currentSubset->getChildren()) {
            child->setParent(nullptr);
        }
        _currentSubset->getChildren().clear();
    }

    // auto newClusterDataset = mv::data().createDataset<Clusters>("Cluster", _clusterDataset->getGuiName(), _currentDataSet);
    _clusterDataset->getClusters() = QVector<Cluster>();
    for (int splitId=0; splitId<numberOfSplits; splitId++) {
        if (_currentSubset == nullptr) this->addSubset();

        auto subset = new Subset(&_functionWidgetAction.getEditSubsetsAction(), "Subset setting", subsetUniqueID);
        subset->initialize(this);
        auto rng = QRandomGenerator::global();
        auto color = QColor::fromHsl(rng->bounded(360), rng->bounded(150, 255), rng->bounded(100, 200));
        subset->setColor(color);
        subset->setName(splitElementName + QString::number(subsetUniqueID));
        subsetUniqueID++;
        subset->setIndices(splitIndicesVector[splitId]);
        subset->setChannelId(channelId);

        Cluster cluster;
        cluster.setName(subset->getName());
        cluster.setColor(subset->getColorAction().getColor());
        cluster.setIndices(subset->getIndices());
        _clusterDataset->addCluster(cluster);

        // update cluster's color in data hierarchy
        // for (auto cluster: _clusterDataset->getClusters()) {
        //     if (cluster.getName() == subset->getName()) {
        //         qDebug() << "before: " << cluster.getColor();
        //         cluster.setColor(color);
        //         qDebug() << "after: " << cluster.getColor();
        //         break;
        //     }
        // }
        

        std::vector<float> data;
        data.resize(splitIndicesVector[splitId].size());
        _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, splitIndicesVector[splitId]);
        subset->setMean(meanCalculation(data));
        subset->setStd(sdCalculation(data));
        // subset->setWeight(w[splitId]);

        // set current selection as ROI
        _currentSubset->addChild(subset);

        _subsetModel->addSubset(subset);
    }

    // mv::data().removeDataset(_clusterDataset);
    // _clusterDataset = newClusterDataset;
    // mv::events().notifyDatasetDataChanged(_clusterDataset);

    events().notifyDatasetDataChanged(_clusterDataset);
    _clusterDataset = mv::Dataset<Clusters>(nullptr);

    auto root = _currentSubset->getRoot();
    QVariantMap rootData = createHierarchyMap(root);
    emit _chartWidget->getCommunicationObject().qt_js_setTreeData(rootData);
}

void XRFAnalysisPlugin::splitGaussians(const QVariantMap& clicked) {
    return;
    auto splitElementName = clicked["element"].toString();
    int channelId = _channel_ID_map[splitElementName];
    int subsetId = clicked["subsetId"].toInt();
    qDebug() << "qt double click on " << splitElementName;

    std::vector<uint> selectionIndices;
    int selectionSize;
    if (subsetId < 0) {
        selectionIndices = _currentDataSet->getSelectionIndices();
        selectionSize = _currentDataSet->getSelectionSize();
    }
    else {
        for (auto subsetIt: _subsetModel->getSubsets()) {
            if (subsetIt->getId() == subsetId) {
                selectionIndices = subsetIt->getIndices();
                selectionSize = selectionIndices.size();
            }
        }
    }

    QVariantMap gaussians;
    QVariantList means, sigmas, weights;
    gaussians["means"] = means;
    gaussians["sigmas"] = sigmas;
    gaussians["weights"] = weights;

    std::vector<float> dataSelection;
    dataSelection.resize(selectionSize);
    _currentDataSet->populateDataForDimensions(dataSelection, std::vector<int>{channelId}, selectionIndices);

    int bestFit = 2;
    GMM gmm(dataSelection);
    gmm.gmm(bestFit);

    qDebug() << "best fit: " << bestFit;
    auto m = gmm.getMeans(bestFit);
    auto s = gmm.getSigmas(bestFit);
    auto w = gmm.getWeights(bestFit);
    for (int i=0; i<bestFit; i++) {
        means.append(m[i]);
        sigmas.append(s[i]);
        weights.append(w[i]);
    }
    gaussians["means"] = means;
    gaussians["sigmas"] = sigmas;
    gaussians["weights"] = weights;
    gmm.calculateCuts(bestFit);
    _cuts = gmm.getCuts();
    // emit _chartWidget->getCommunicationObject().qt_js_setGaussians(gaussians);
    
    int numberOfSplits = _cuts.size() + 1;
    
    QVariantList splits;
    QVariantMap split;
    

    auto subsetIndices = _currentDataSet->getSelectionIndices();
    if (subsetId >= 0) {
        for (auto subsetIt: _subsetModel->getSubsets()) {
            if (subsetIt->getId() == subsetId) {
                subsetIndices = subsetIt->getIndices();
                break;
            }
        }
    }
    int subsetSize = subsetIndices.size();

    std::vector<float> dataFull;
    _currentDataSet->extractDataForDimension(dataFull, channelId);

    std::vector<std::vector<uint32_t>> splitIndicesVector(numberOfSplits);
    for (int splitId = 0; splitId < numberOfSplits; splitId++) {
        splitIndicesVector[splitId].reserve(subsetSize);
    }
    for (auto index: subsetIndices) {
        float val = dataFull[index];
        for (int splitId=0; splitId<numberOfSplits-1; splitId++) {
            if (val < _cuts[splitId]) {
                splitIndicesVector[splitId].push_back(index);
                break;
            }
            else if (val >= _cuts[numberOfSplits-2]) {
                splitIndicesVector[numberOfSplits-1].push_back(index);
                break;
            }
        }
    }

    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        split.clear();
        split["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);

        float min = std::numeric_limits<float>::infinity();
        float max = -std::numeric_limits<float>::infinity();
        std::vector<float> dataSubset(subsetSize);
        _currentDataSet->populateDataForDimensions(dataSubset, std::vector<int>{channelId}, subsetIndices);
        for (auto val: dataSubset) {
            min = (min > val) ? val : min;
            max = (max < val) ? val : max;
        }
        QVariantList values;
        for (int splitId=0; splitId<numberOfSplits; splitId++) {
            const auto& splitIndices = splitIndicesVector[splitId];
            int splitSize = splitIndices.size();
            QVariantMap splitValue;
            splitValue["split"] = splitElementName + QString::number(splitId);
            std::vector<float> data;
            data.resize(splitSize);
            _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, splitIndices);

            float avg = meanCalculation(data);
            avg = (avg - min) / (max - min);
            if (max == min) avg = 0.0;
            splitValue["value"] = avg;

            values.append(splitValue);
        }
        split["values"] = values;
        splits.append(split);
    }

    if (_currentSubset) {
        for (auto child: _currentSubset->getChildren()) {
            child->setParent(nullptr);
        }
        _currentSubset->getChildren().clear();
    }
    for (int splitId=0; splitId<numberOfSplits; splitId++) {
        if (_currentSubset == nullptr) this->addSubset();

        auto subset = new Subset(&_functionWidgetAction.getEditSubsetsAction(), "Subset setting", subsetUniqueID);
        subset->initialize(this);
        // subset->setVisibility(true);
        auto rng = QRandomGenerator::global();
        subset->setColor(QColor::fromHsl(rng->bounded(360), rng->bounded(150, 255), rng->bounded(100, 200)));
        subset->setName(splitElementName + QString::number(subsetUniqueID));
        subsetUniqueID++;
        subset->setIndices(splitIndicesVector[splitId]);
        subset->setChannelId(channelId);
        subset->setMean(m[splitId]);
        subset->setStd(s[splitId]);
        subset->setWeight(w[splitId]);

        // set current selection as ROI
        _currentSubset->addChild(subset);

        _subsetModel->addSubset(subset);
    }

    auto root = _currentSubset->getRoot();
    QVariantMap rootData = createHierarchyMap(root);
    emit _chartWidget->getCommunicationObject().qt_js_setTreeData(rootData);

    // emit _chartWidget->getCommunicationObject().qt_js_setSplits(splits);
}

QVariantMap XRFAnalysisPlugin::createHierarchyMap(Subset* node) {
    QVariantMap nodeData;
    if (node == nullptr) return nodeData;
    nodeData["name"] = node->getName();
    nodeData["id"] = node->getId();
    nodeData["color"] = node->getColorAction().getColor().name();
    nodeData["mean"] = node->getMean();
    nodeData["std"] = node->getStd();
    nodeData["weight"] = node->getWeight();

    // high value elements for this subset
    if (node->getParent() != nullptr) {
        std::vector<float> relativeMean(_numOfChannels);
        std::vector<QFuture<void>> futureVector(_numOfChannels);
        for (int channelId = 0; channelId < _numOfChannels; channelId++) {
            auto future = QtConcurrent::run([this, channelId, node, &relativeMean]() {
                std::vector<float> data;
                data.resize(node->getSubsetSize());
                _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, node->getIndices());
                float avg = meanCalculation(data);

                Subset* parent = node->getParent();
                std::vector<float> dataParent;
                dataParent.resize(parent->getSubsetSize());
                _currentDataSet->populateDataForDimensions(dataParent, std::vector<int>{channelId}, parent->getIndices());
                float avgAll = meanCalculation(dataParent);
                // float avgAll = _channelMean[channelId];
            
                avg = (avg - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
            
                avgAll = (avgAll - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
                
                if (_channelMaxima[channelId] == _channelMinima[channelId]) avg = 0.0;
                relativeMean[channelId] = avg - avgAll;
            });
            
            futureVector[channelId] = future;
        }
        for (int channelId = 0; channelId < _numOfChannels; channelId++) {
            futureVector[channelId].waitForFinished();
        }
        std::vector<int> sortedChannelIndices(_numOfChannels);
        std::iota(sortedChannelIndices.begin(), sortedChannelIndices.end(), 0);
        std::sort(sortedChannelIndices.begin(), sortedChannelIndices.end(), [&relativeMean](int i, int j) { 
            return relativeMean[i] > relativeMean[j];
        });
        float thres = _statisticsAction.getRelativeMeanTreeAction().getValue();
        QVariantList highConcentrations;
        int cnt = 0;
        for (auto id: sortedChannelIndices) {
            if (relativeMean[id] > thres && cnt < 4) {
                highConcentrations.push_back(_currentDataSet->getDimensionNames()[id].first(3));
                cnt += 1;
            }
            else break;
        }
        nodeData["highConcentrations"] = highConcentrations;
    } 

    QVariantList childrenData;
    for (auto child: node->getChildren()) {
        QVariantMap childData = createHierarchyMap(child);
        childrenData.push_back(childData);
    }
    nodeData["children"] = childrenData;
    return nodeData;
}

void XRFAnalysisPlugin::calculateFocusingElementDetail(const QVariantMap &focusing)
{
    int channelId = _channel_ID_map[focusing["element"].toString()];
    int subsetId = focusing["subsetId"].toInt();
    std::vector<uint> selectionIndices;
    int selectionSize;
    if (subsetId < 0) {
        selectionIndices = _currentDataSet->getSelectionIndices();
        selectionSize = _currentDataSet->getSelectionSize();
    }
    else {
        for (auto subsetIt: _subsetModel->getSubsets()) {
            if (subsetIt->getId() == subsetId) {
                selectionIndices = subsetIt->getIndices();
                selectionSize = selectionIndices.size();
            }
        }
    }

    std::vector<float> dataSelection;
    dataSelection.resize(selectionSize);
    _currentDataSet->populateDataForDimensions(dataSelection, std::vector<int>{channelId}, selectionIndices);

    std::vector<float> dataFull;
    _currentDataSet->extractDataForDimension(dataFull, channelId);


    const auto normalizeHistogram = [](std::vector<int> &arr) -> std::vector<float>
    {
        int max = -std::numeric_limits<int>::infinity();
        for (int val : arr)
            max = std::max(max, val);
        float maxFloat = static_cast<float>(max);

        // int sum = 0;
        // for (int val: arr)
        //     sum += val;
        // float sumFloat = static_cast<float>(sum);

        int size = arr.size();
        std::vector<float> histogramNormalized(size);
        for (int i = 0; i < size; i++)
        {
            float val = static_cast<float>(arr[i]);
            histogramNormalized[i] = val / maxFloat;
        }

        return histogramNormalized;
    };

    int statisticsType = _statisticsAction.getStatisticsAction().getCurrentIndex();
    QVariantList histograms_qvariant, histogramSelection_qvariant, histogramFull_qvariant;

    QVariantMap gaussians;
    QVariantList means, sigmas, weights;
    gaussians["means"] = means;
    gaussians["sigmas"] = sigmas;
    gaussians["weights"] = weights;

    if (statisticsType == 0 || statisticsType == 1)
    {
        // avg value
        float min = _channelMinima[channelId];
        float max = _channelMaxima[channelId];
        auto histogramSelection = histogramCalculation(
            dataSelection, 
            min, 
            max, 
            HISTOGRAM_RESOLUTION, 
            -1
        );
        std::vector<int> histogramFull(histogramSelection.size());
        histogramFull = histogramCalculation(
            dataFull, 
            min, 
            max, 
            HISTOGRAM_RESOLUTION
        );
        for (const auto &i : normalizeHistogram(histogramSelection))
            histogramSelection_qvariant.append(QVariant(i));
        for (const auto &i : normalizeHistogram(histogramFull))
            histogramFull_qvariant.append(QVariant(i));
        
    }
    else if (statisticsType == 4) {
        // number of peaks
        float min = _channelMinima[channelId];
        float max = _channelMaxima[channelId];
        auto histogramSelection = histogramCalculation(
            dataSelection, 
            min, 
            max, 
            HISTOGRAM_RESOLUTION, 
            -1
        );
        std::vector<int> histogramFull(histogramSelection.size());

        for (const auto &i : histogramSelection)
            histogramSelection_qvariant.append(QVariant(i));
        for (const auto &i : histogramFull)
            histogramFull_qvariant.append(QVariant(i));
    }
    else if (statisticsType >= 2)
    {
        // multimodal/variance
        float min = _channelMinima[channelId];
        float max = _channelMaxima[channelId];
        auto histogramSelection = histogramCalculation(
            dataSelection, 
            min, 
            max, 
            HISTOGRAM_RESOLUTION, 
            -1
        );
        std::vector<int> histogramFull(histogramSelection.size());

        for (const auto &i : histogramSelection)
            histogramSelection_qvariant.append(QVariant(i));
        for (const auto &i : histogramFull)
            histogramFull_qvariant.append(QVariant(i));
    }
    
    
    histograms_qvariant.append(histogramSelection_qvariant);
    histograms_qvariant.append(histogramFull_qvariant);
    
    // emit _chartWidget->getCommunicationObject().qt_js_setGaussians(gaussians);
    emit _chartWidget->getCommunicationObject().qt_js_setHistograms(histograms_qvariant);
}

void XRFAnalysisPlugin::publishSelection(const std::vector<unsigned int> &selectedIDs)
{
    // ask core for the selection set for the current data set
    auto selectionSet = _currentDataSet->getSelection<Points>();
    auto &selectionIndices = selectionSet->indices;

    // clear the selection and add the new points
    selectionIndices.clear();
    selectionIndices.reserve(_currentDataSet->getNumPoints());
    for (const auto id : selectedIDs)
    {
        selectionIndices.push_back(id);
    }

    // notify core about the selection change
    if (_currentDataSet->isDerivedData())
        events().notifyDatasetDataSelectionChanged(_currentDataSet->getSourceDataset<DatasetImpl>());
    else
        events().notifyDatasetDataSelectionChanged(_currentDataSet);
}

QString XRFAnalysisPlugin::getCurrentDataSetID() const
{
    if (_currentDataSet.isValid())
        return _currentDataSet->getId();
    else
        return QString{};
}

// =============================================================================
// Plugin Factory
// =============================================================================

XRFAnalysisPluginFactory::XRFAnalysisPluginFactory()
{
    setIconByName("bullseye");

    getPluginMetadata().setDescription("XRFAnalysisPlugin Javascript view plugin");
    getPluginMetadata().setSummary("This plugin shows how to implement a basic Javascript-based view plugin in ManiVault Studio.");
    getPluginMetadata().setCopyrightHolder({"BioVault (Biomedical Visual Analytics Unit LUMC - TU Delft)"});
    getPluginMetadata().setAuthors({{"A. Vieth", {"Plugin developer", "Maintainer"}, {"LUMC", "TU Delft"}},
                                    {"J. Thijssen", {"Software architect"}, {"LUMC", "TU Delft"}},
                                    {"T. Kroes", {"Lead software architect"}, {"LUMC"}}});
    getPluginMetadata().setOrganizations({{"LUMC", "Leiden University Medical Center", "https://www.lumc.nl/en/"},
                                          {"TU Delft", "Delft university of technology", "https://www.tudelft.nl/"}});
    getPluginMetadata().setLicenseText("This plugin is distributed under the [LGPL v3.0](https://www.gnu.org/licenses/lgpl-3.0.en.html) license.");
}

ViewPlugin *XRFAnalysisPluginFactory::produce()
{
    return new XRFAnalysisPlugin(this);
}

mv::DataTypes XRFAnalysisPluginFactory::supportedDataTypes() const
{
    // This example analysis plugin is compatible with points datasets
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

mv::gui::PluginTriggerActions XRFAnalysisPluginFactory::getPluginTriggerActions(const mv::Datasets &datasets) const
{
    PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> XRFAnalysisPlugin *
    {
        return dynamic_cast<XRFAnalysisPlugin *>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets == 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType))
    {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<XRFAnalysisPluginFactory *>(this), this, "XRFAnalysisPlugin", "View JavaScript visualization", icon(), [this, getPluginInstance, datasets](PluginTriggerAction &pluginTriggerAction) -> void
                                                           {
                                                               for (auto dataset : datasets)
                                                                   getPluginInstance()->loadData(Datasets({dataset}));
                                                           });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}
