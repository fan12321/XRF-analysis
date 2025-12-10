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

Q_PLUGIN_METADATA(IID "studio.manivault.XRFAnalysisPlugin")

using namespace mv;

const int HISTOGRAM_RESOLUTION = 128;

int numberOfPeaksCalculation(std::vector<float> &arr, int resolution = HISTOGRAM_RESOLUTION, bool smooth = true)
{
    if (arr.empty())
        return 0;

    float min = std::numeric_limits<float>::infinity();
    float max = -std::numeric_limits<float>::infinity();
    for (float val : arr)
    {
        min = std::min(min, val);
        max = std::max(max, val);
    }

    if (min == max)
        return 0;

    std::vector<int> histogram(resolution);
    int peakValue = 0;
    for (float val : arr)
    {
        int bucketId = static_cast<int>((resolution - 1) * (val - min) / (max - min));
        histogram[bucketId] += 1;

        if (histogram[bucketId] > peakValue)
            peakValue = histogram[bucketId];
    }

    if (smooth)
    {
        int size = (resolution >> 3) - 1;
        int center = size >> 1;
        float sigma = size * 0.16666;
        std::vector<float> gaussian(size);
        for (int i = 0; i < size; i++)
        {
            float x = i - center;
            gaussian[i] = std::exp(-(x * x) / (2.0 * sigma * sigma)) / (sigma * std::sqrt(2.0 * 3.14159));
        }

        std::vector<float> histogramSmoothed(resolution);
        float peakValueSmoothed = 0.0;
        for (int i = 0; i < resolution; i++)
        {
            float val = 0.0;
            for (int j = 0; j < size; j++)
            {
                int idx = i + j - center;
                if (idx >= 0 && idx < resolution)
                {
                    val += static_cast<float>(histogram[idx]) * gaussian[j];
                }
            }
            histogramSmoothed[i] = val;
            if (histogramSmoothed[i] > peakValueSmoothed)
                peakValueSmoothed = histogramSmoothed[i];
        }

        int cnt = 0;
        if (histogramSmoothed[0] > histogramSmoothed[1])
            cnt += 1;
        for (int i = 1; i < resolution - 1; i++)
        {
            if (
                histogramSmoothed[i] > histogramSmoothed[i - 1] &&
                histogramSmoothed[i] > histogramSmoothed[i + 1] &&
                histogramSmoothed[i] > peakValueSmoothed * 0.2)
            {
                cnt += 1;
            }
        }

        return cnt;
    }

    int cnt = 0;
    if (histogram[0] > histogram[1])
        cnt += 1;
    for (int i = 1; i < resolution - 1; i++)
    {
        if (
            histogram[i] > histogram[i - 1] &&
            histogram[i] > histogram[i + 1] &&
            histogram[i] > peakValue * 0.05)
        {
            cnt += 1;
        }
    }

    return cnt;
};

XRFAnalysisPlugin::XRFAnalysisPlugin(const PluginFactory *factory) : ViewPlugin(factory),
                                                                     _subsetModel(new SubsetModel(this)),
                                                                     _selectionModel(_subsetModel),
                                                                     _currentSubset(nullptr), 
                                                                     _chartWidget(nullptr),
                                                                     _dropWidget(nullptr),
                                                                     _currentDataSet(nullptr),
                                                                     _statisticsAction(this, "Statistics"),
                                                                     _addSubsetAction(this, "Set ROI"),
                                                                     _primaryToolbarAction(this, "Primary toolbar"),
                                                                     _functionWidgetAction(this, "Functions")
{
    getLearningCenterAction().addVideos(QStringList({"Practitioner", "Developer"}));

    _primaryToolbarAction.setDefaultWidgetFlags(GroupAction::Horizontal);
    _primaryToolbarAction.addAction(&_statisticsAction);
    _primaryToolbarAction.addAction(&_addSubsetAction);
}

void XRFAnalysisPlugin::init()
{
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
        const auto dataTypes = DataTypes({ PointType });

        if (dataTypes.contains(dataType)) {

            if (datasetId == getCurrentDataSetID()) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else {
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

    // update data when data set changed
    connect(&_currentDataSet, &Dataset<Points>::dataChanged, this, &XRFAnalysisPlugin::convertDataAndUpdateChart);
    connect(&_currentDataSet, &Dataset<Points>::dataSelectionChanged, this, &XRFAnalysisPlugin::convertDataAndUpdateChart);
    connect(&_statisticsAction.getStatisticsAction(), &OptionAction::currentIndexChanged, this, &XRFAnalysisPlugin::convertDataAndUpdateChart);

    connect(&_statisticsAction.getRelativeValueAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getMeanValueAction(), &DecimalAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);
    connect(&_statisticsAction.getNumberOfPeaksAction(), &IntegralAction::valueChanged, this, &XRFAnalysisPlugin::updateThreshold);

    connect(&_addSubsetAction, &TriggerAction::triggered, this, &XRFAnalysisPlugin::addSubset);

    // Update the selection (coming from PCP) in core
    // connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &XRFAnalysisPlugin::publishSelection);

    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passFocusingElementToCore, this, &XRFAnalysisPlugin::calculateFocusingElementDetail);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passDblClickSplit, this, &XRFAnalysisPlugin::splitGaussians);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passNodeId, this, [this](const QVariantMap& data) {
        int subsetId = data["id"].toInt();
        for (auto subset: _subsetModel->getSubsets()) {
            if (subset->getId() == subsetId) {
                _currentSubset = subset;
                return;
            }
        }
    });
    // addNotification(getExampleNotificationMessage());
}

void XRFAnalysisPlugin::loadData(const mv::Datasets &datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    qDebug() << "XRFAnalysisPlugin::loadData: Load data set from ManiVault core";

    // Load the first dataset, changes to _currentDataSet are connected with convertDataAndUpdateChart
    _currentDataSet = datasets.first();
    _numOfChannels = _currentDataSet->getNumDimensions();
    _percentiles = std::vector<std::vector<float>>(_numOfChannels);
    _channelMaxima = std::vector<float>(_numOfChannels);
    _channelMinima = std::vector<float>(_numOfChannels);
    auto channelNames = _currentDataSet->getDimensionNames();
    int size = _currentDataSet->getNumPoints();
    int stripe = size / 100;

    std::vector<QFuture<void>> futureVector(_numOfChannels);
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        auto future = QtConcurrent::run([this, channelId, stripe, size]() {
            std::vector<float> percentile(100);
            std::vector<float> data;
            _currentDataSet->extractDataForDimension(data, channelId);
            std::sort(data.begin(), data.end());
    
            for (int i=0; i<100; i++) {
                percentile[i] = data[i*stripe];
            }
            _percentiles[channelId].assign(percentile.begin(), percentile.end());
    
            _channelMaxima[channelId] = data[size-1];
            _channelMinima[channelId] = data[0];
        });
        QString channelName = channelNames[channelId].first(3);
        _channel_ID_map.insert({channelName, channelId});
        futureVector[channelId] = future;
    }
    for (int channelId = 0; channelId < _numOfChannels; channelId++) {
        futureVector[channelId].waitForFinished();
    }

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
        threshold = _statisticsAction.getRelativeValueAction().getValue();
    }
    else if (type == 1)
    {
        threshold = _statisticsAction.getMeanValueAction().getValue();
    }
    else if (type == 2)
    {
        threshold = _statisticsAction.getNumberOfPeaksAction().getValue();
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
    subset->setVisibility(false);
    auto rng = QRandomGenerator::global();
    subset->setColor(QColor::fromHsl(rng->bounded(360), rng->bounded(150, 255), rng->bounded(100, 200)));
    subset->setName("ROI_" + QString::number(subsetUniqueID));
    subsetUniqueID++;
    subset->setIndices(_currentDataSet->getSelectionIndices());
    _subsetModel->addSubset(subset);
    _currentSubset = subset;
    // convertDataAndUpdateChart();
}

void XRFAnalysisPlugin::convertDataAndUpdateChart()
{
    _currentSubset = nullptr;
    if (!_currentDataSet.isValid())
        return;

    qDebug() << "XRFAnalysisPlugin::convertDataAndUpdateChart: Prepare payload";

    // convert data from ManiVault PointData to a JSON structure
    QVariantList subsets;
    QVariantMap subset;

    const auto meanCalculation = [](std::vector<float> &arr)
    {
        if (arr.empty())
            return 0.0f;

        auto count = static_cast<float>(arr.size());
        return std::reduce(arr.begin(), arr.end()) / count;
    };

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

        if (statisticsType == 0) {
            if (selectionSize == 0) {
                elementValue["value"] = 0.0;
            }
            else {
                float avg = meanCalculation(data);
                elementValue["value"] = 1.0;
                for (int step=0; step<100; step++) {
                    if (avg < _percentiles[channelId][step]) {
                        elementValue["value"] = static_cast<float>(step) * 0.01;
                        break;
                    }
                }
            }
        }
        else if (statisticsType == 1)
        {
            float avg = meanCalculation(data);
            avg = (avg - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
            if (_channelMaxima[channelId] == _channelMinima[channelId]) avg = 0.0;
            elementValue["value"] = avg;
        }
        else if (statisticsType == 2)
        {
            bool smoothed = true;
            int peaksCount = numberOfPeaksCalculation(data, HISTOGRAM_RESOLUTION, smoothed);
            elementValue["value"] = peaksCount;
        }

        values.append(elementValue);
    }
    subset["values"] = values;
    subsets.append(subset);

    for (auto subsetIt: _subsetModel->getSubsets())
    {
        if (subsetIt->getVisibleAction().isChecked())
        {
            subset.clear();
            subset["subset"] = subsetIt->getName();
            subset["id"] = subsetIt->getId();
            QVariantList values;
            auto selectionIndices = subsetIt->getIndices();
            int selectionSize = selectionIndices.size();

            for (int channelId = 0; channelId < _numOfChannels; channelId++)
            {
                QVariantMap elementValue;
                elementValue["element"] = _currentDataSet->getDimensionNames()[channelId].first(3);
                std::vector<float> data;
                data.resize(selectionSize);
                _currentDataSet->populateDataForDimensions(data, std::vector<int>{channelId}, selectionIndices);

                auto statisticsType = _statisticsAction.getStatisticsAction().getCurrentIndex();
                if (statisticsType == 0) {
                    if (selectionSize == 0) {
                        elementValue["value"] = 0.0;
                    }
                    else {
                        float avg = meanCalculation(data);
                        elementValue["value"] = 1.0;
                        for (int step=0; step<100; step++) {
                            if (avg < _percentiles[channelId][step]) {
                                elementValue["value"] = static_cast<float>(step) * 0.01;
                                break;
                            }
                        }
                    }
                }
                else if (statisticsType == 1)
                {
                    float avg = meanCalculation(data);
                    avg = (avg - _channelMinima[channelId]) / (_channelMaxima[channelId] - _channelMinima[channelId]);
                    if (_channelMaxima[channelId] == _channelMinima[channelId]) avg = 0.0;
                    elementValue["value"] = avg;
                }
                else if (statisticsType == 2)
                {
                    bool smoothed = true;
                    int peaksCount = numberOfPeaksCalculation(data, HISTOGRAM_RESOLUTION, smoothed);
                    elementValue["value"] = peaksCount;
                }

                values.append(elementValue);
            }
            subset["values"] = values;
            subsets.append(subset);
        }
    }

    qDebug() << "XRFAnalysisPlugin::convertDataAndUpdateChart: Send data from Qt cpp to D3 js";
    updateThreshold();
    emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(subsets);
}

void XRFAnalysisPlugin::splitGaussians(const QVariantMap& clicked) {
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

    GMM gmm(dataSelection);
    int bestFit = gmm.bestFit();
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
    const auto meanCalculation = [](std::vector<float> &arr)
    {
        if (arr.empty())
        return 0.0f;
        
        auto count = static_cast<float>(arr.size());
        return std::reduce(arr.begin(), arr.end()) / count;
    };
    
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
            }
            else if (val >= _cuts[numberOfSplits-2]) {
                splitIndicesVector[numberOfSplits-1].push_back(index);
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

    for (int splitId=0; splitId<numberOfSplits; splitId++) {
        if (_currentSubset == nullptr) this->addSubset();

        auto subset = new Subset(&_functionWidgetAction.getEditSubsetsAction(), "Subset setting", subsetUniqueID);
        subset->initialize(this);
        subset->setVisibility(false);
        auto rng = QRandomGenerator::global();
        subset->setColor(QColor::fromHsl(rng->bounded(360), rng->bounded(150, 255), rng->bounded(100, 200)));
        subset->setName(splitElementName + QString::number(subsetUniqueID));
        subsetUniqueID++;
        subset->setIndices(splitIndicesVector[splitId]);
        subset->setMean(m[splitId]);
        subset->setStd(s[splitId]);
        subset->setWeight(w[splitId]);

        // set current selection as ROI
        _currentSubset->addChild(subset);

        _subsetModel->addSubset(subset);
    }

    auto root = _currentSubset;
    while (root->getParent() != nullptr) root = root->getParent();

    QVariantMap rootData = createHierarchyMap(root);
    emit _chartWidget->getCommunicationObject().qt_js_setTreeData(rootData);

    emit _chartWidget->getCommunicationObject().qt_js_setSplits(splits);
}

QVariantMap XRFAnalysisPlugin::createHierarchyMap(Subset* node) {
    QVariantMap nodeData;
    nodeData["name"] = node->getName();
    nodeData["id"] = node->getId();
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

    const auto histogramCalculation_background = [](std::vector<float> &arr, float min, float max, int resolution = HISTOGRAM_RESOLUTION) -> std::vector<int>
    {
        std::vector<int> histogram(resolution);
        if (min == max)
            return histogram;
        for (float val : arr)
        {
            int bucketId = static_cast<int>((resolution - 1) * (val - min) / (max - min));
            histogram[bucketId] += 1;
        }

        return histogram;
    };

    const auto histogramCalculation = [](std::vector<float> &arr, int resolution = HISTOGRAM_RESOLUTION, bool smooth = false) -> std::vector<int>
    {
        float min = std::numeric_limits<float>::infinity();
        float max = -std::numeric_limits<float>::infinity();
        for (float val : arr)
        {
            min = std::min(min, val);
            max = std::max(max, val);
        }

        std::vector<int> histogram(resolution);
        if (min == max)
            return histogram;
        for (float val : arr)
        {
            int bucketId = static_cast<int>((resolution - 1) * (val - min) / (max - min));
            histogram[bucketId] += 1;
        }

        if (smooth)
        {
            int size = (resolution >> 3) - 1;
            int center = size >> 1;
            float sigma = size * 0.16666;
            std::vector<float> gaussian(size);
            for (int i = 0; i < size; i++)
            {
                float x = i - center;
                gaussian[i] = std::exp(-(x * x) / (2.0 * sigma * sigma)) / (sigma * std::sqrt(2.0 * 3.14159));
            }

            std::vector<int> histogramSmoothed(resolution);
            for (int i = 0; i < resolution; i++)
            {
                float val = 0.0;
                for (int j = 0; j < size; j++)
                {
                    int idx = i + j - center;
                    if (idx >= 0 && idx < resolution)
                    {
                        val += static_cast<float>(histogram[idx]) * gaussian[j];
                    }
                }
                histogramSmoothed[i] = static_cast<int>(val);
            }
            return histogramSmoothed;
        }
        else
            return histogram;
    };

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
    // if (statisticsType == 0 || statisticsType == 1)
    // {
        // relative val or avg
        float min = _channelMinima[channelId];
        float max = _channelMaxima[channelId];
        auto histogramSelection = histogramCalculation_background(dataSelection, min, max);
        auto histogramFull = histogramCalculation_background(dataFull, min, max);
        for (const auto &i : normalizeHistogram(histogramSelection))
            histogramSelection_qvariant.append(QVariant(i));
        for (const auto &i : normalizeHistogram(histogramFull))
            histogramFull_qvariant.append(QVariant(i));
    // }
    // else if (statisticsType == 2)
    // {
    //     // peaks
    //     bool smoothed = true;
    //     auto histogramSelection = histogramCalculation(dataSelection, HISTOGRAM_RESOLUTION, false);
    //     std::vector<int> histogramFull(histogramSelection.size());

    //     for (const auto &i : histogramSelection)
    //         histogramSelection_qvariant.append(QVariant(i));
    //     for (const auto &i : histogramFull)
    //         histogramFull_qvariant.append(QVariant(i));

        
    //     int peaksCount = numberOfPeaksCalculation(dataSelection, HISTOGRAM_RESOLUTION, smoothed);
    //     if (peaksCount > 1) {
    //         GMM gmm(dataSelection);
    //         int bestFit = gmm.bestFit();
    //         qDebug() << "best fit: " << bestFit;
    //         auto m = gmm.getMeans(bestFit);
    //         auto s = gmm.getSigmas(bestFit);
    //         auto w = gmm.getWeights(bestFit);
    //         for (int i=0; i<bestFit; i++) {
    //             means.append(m[i]);
    //             sigmas.append(s[i]);
    //             weights.append(w[i]);
    //         }
    //         gaussians["means"] = means;
    //         gaussians["sigmas"] = sigmas;
    //         gaussians["weights"] = weights;
    //         gmm.calculateCuts(bestFit);
    //         _cuts = gmm.getCuts();
    //         emit _chartWidget->getCommunicationObject().qt_js_setGaussians(gaussians);
    //     }
    // }
    
    
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
