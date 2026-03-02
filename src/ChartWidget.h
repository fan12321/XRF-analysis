#pragma once 

#include "widgets/WebWidget.h"

#include <QVariantList>

Q_DECLARE_METATYPE(QVariantList)

class XRFAnalysisPlugin;

// =============================================================================
// ParlCoorCommunicationObject
// =============================================================================

class ChartCommObject : public mv::gui::WebCommunicationObject
{
    Q_OBJECT
public:
    ChartCommObject();

signals:
    // Signals from Qt to JS side
    // This specific signal is used to transfer data from ManiVault to the D3 plot
    // But other communication like messaging selection IDs can be handled the same
    void qt_js_setChannelInfo(const QVariantList& data);
    void qt_js_setDataAndPlotInJS(const QVariantList& data);
    void qt_js_setThreshold(const QVariantList& data);
    void qt_js_setHistograms(const QVariantList& data);
    void qt_js_setGaussians(const QVariantMap& data);
    void qt_js_setSplits(const QVariantList& data);
    void qt_js_setPreviewSplits(const QVariantList& data);
    void qt_js_setTreeData(const QVariantMap& data);
    void qt_js_setFocusNodeId(const QVariant& data);
    void qt_js_setMatchingElements(const QVariant& data);

    // Signals Qt internal
    // Used to inform the plugin about new selection: the plugin class then updates ManiVault's core
    void passSelectionToCore(const std::vector<unsigned int>& selectionIDs);
    void passFocusingElementToCore(const QVariantMap& subset_element);
    void passDblClickSplit(const QVariantMap& gaussianSplit);
    void passSplitCuts(const QVariantMap& cuts);
    void passPreviewSplits(const QVariantMap& cuts);
    void passNoSplitsSignal();
    void passNodeId(const QVariantMap& subsetId);
    void passParentNodeId(const QVariantMap& id);
    void passCompareClustersSignal();
    void passMatchingElements(const QVariantMap& elements);
    void passElementToImageViewer(const QVariantMap& element);

public slots:
    // Invoked from JS side 
    // Used to receive selection IDs from the D3 plot, will emit passSelectionToCore
    void js_qt_passSelectionToQt(const QVariantList& data);
    void js_qt_passFocusingElementToQt(const QVariantMap& data);
    void js_qt_passDblClickSplitToQt(const QVariantMap& data);
    void js_qt_passNodeIdToQt(const QVariantMap& data);
    void js_qt_passSplitCutsToQt(const QVariantMap& data);
    void js_qt_passPreviewSplitsToQt(const QVariantMap& data);
    void js_qt_passNoSplitsSignalToQt();
    void js_qt_passParentNodeIdToQt(const QVariantMap& data);
    void js_qt_passCompareClustersSignalToQt();
    void js_qt_passMatchingElementsToQt(const QVariantMap& data);
    void js_qt_passElementToQtImageViewer(const QVariantMap& data);

private:
    std::vector<unsigned int> _selectedIDsFromJS;   // Used for converting incoming selection IDs from the js side
};


// =============================================================================
// ChartWidget
// =============================================================================

class ChartWidget : public mv::gui::WebWidget
{
    Q_OBJECT
public:
    ChartWidget(XRFAnalysisPlugin* viewJSPlugin);

    ChartCommObject& getCommunicationObject() { return _comObject; };

private slots:
    /** Is invoked when the js side calls js_available of the mv::gui::WebCommunicationObject (ChartCommObject) 
        js_available emits notifyJsBridgeIsAvailable, which is conencted to this slot in WebWidget.cpp*/
    void initWebPage() override;

private:
    XRFAnalysisPlugin*    _viewJSPlugin;    // Pointer to the main plugin class
    ChartCommObject       _comObject;       // Communication Object between Qt (cpp) and JavaScript
};
