#include "StatisticsAction.h"
#include "XRFAnalysisPlugin.h"

using namespace mv::gui;

StatisticsAction::StatisticsAction(QObject* parent, const QString& title) : 
    GroupAction(parent, title), 
    _XRFAnalysisPlugin(dynamic_cast<XRFAnalysisPlugin*>(parent)), 
    // _statisticsAction(this, "Statistics", {"Mean value", "Relative mean", "Variance", "Median absolute deviation", "Number of peaks", "Normal distribution"}, "Mean value"), 
    _statisticsAction(this, "Statistics", {"Mean value", "Relative mean", "Variance", "Median absolute deviation"}, "Mean value"), 
    // _useQuantileAction(this, "Quantile", false), 
    _relativeMeanTreeAction(this, "Relative mean(tree)", -1.0, 1.0, 1.0, 2), 
    _meanValueAction(this, "Mean", 0.0, 1.0, 0.3, 2), 
    _relativeMeanValueAction(this, "Relative mean", -1.0, 1.0, 0.01, 2), 
    _varianceAction(this, "Variance", 0.00, 0.01, 0.005, 3), 
    _madAction(this, "Median absolute deviation", 0.0, 1.0, 0.03, 2), 
    _numberOfPeaksAction(this, "Peaks", 1, 5, 2), 
    _logLikelihoodAction(this, "Fit", -20.0, 20.0, 0, 2)
{
    setIconByName("arrow-down-wide-short");
    setLabelSizingType(LabelSizingType::Auto);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::HiddenInActionContextMenu);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);

    addAction(&_statisticsAction);
    // addAction(&_relativeMeanTreeAction);
    addAction(&_meanValueAction);
    addAction(&_relativeMeanValueAction);
    addAction(&_varianceAction);
    addAction(&_madAction);
    // addAction(&_numberOfPeaksAction);
    // addAction(&_logLikelihoodAction);

    auto const updateStatistics = [this]() -> void {
        int id = _statisticsAction.getCurrentIndex();
        if (id == 0) {
            _meanValueAction.setEnabled(true);
            _relativeMeanValueAction.setEnabled(false);
            _varianceAction.setEnabled(false);
            _madAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 1) {
            _meanValueAction.setEnabled(false);
            _relativeMeanValueAction.setEnabled(true);
            _varianceAction.setEnabled(false);
            _madAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 2) {
            _meanValueAction.setEnabled(false);
            _relativeMeanValueAction.setEnabled(false);
            _varianceAction.setEnabled(true);
            _madAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 3) {
            _meanValueAction.setEnabled(false);
            _relativeMeanValueAction.setEnabled(false);
            _varianceAction.setEnabled(false);
            _madAction.setEnabled(true);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 4) {
            _meanValueAction.setEnabled(false);
            _relativeMeanValueAction.setEnabled(false);
            _varianceAction.setEnabled(false);
            _madAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(true);
            _logLikelihoodAction.setEnabled(false);
        }
        else {
            _meanValueAction.setEnabled(false);
            _relativeMeanValueAction.setEnabled(false);
            _varianceAction.setEnabled(false);
            _madAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(true);
        }
    };

    connect(&_statisticsAction, &OptionAction::currentIndexChanged, this, updateStatistics);
    _statisticsAction.setEnabled(true);
    _statisticsAction.setCurrentIndex(0);
    updateStatistics();
}

