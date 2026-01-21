#include "StatisticsAction.h"
#include "XRFAnalysisPlugin.h"

using namespace mv::gui;

StatisticsAction::StatisticsAction(QObject* parent, const QString& title) : 
    GroupAction(parent, title), 
    _XRFAnalysisPlugin(dynamic_cast<XRFAnalysisPlugin*>(parent)), 
    _statisticsAction(this, "Statistics", {"Mean value", "Number of peaks", "Normal distribution"}, "Mean value"), 
    _useQuantileAction(this, "Quantile", false), 
    _meanValueAction(this, "Mean", 0.0, 1.0, 0.5, 2), 
    _numberOfPeaksAction(this, "Peaks", 1, 5, 2), 
    _logLikelihoodAction(this, "Fit", -20.0, 20.0, 0, 2)
{
    setIconByName("arrow-down-wide-short");
    setLabelSizingType(LabelSizingType::Auto);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::HiddenInActionContextMenu);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);

    addAction(&_statisticsAction);
    addAction(&_useQuantileAction);
    addAction(&_meanValueAction);
    addAction(&_numberOfPeaksAction);
    addAction(&_logLikelihoodAction);

    auto const updateStatistics = [this]() -> void {
        int id = _statisticsAction.getCurrentIndex();
        if (id == 0) {
            _meanValueAction.setEnabled(true);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 1) {
            _meanValueAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(true);
            _logLikelihoodAction.setEnabled(false);
        }
        else if (id == 2) {
            _meanValueAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
            _logLikelihoodAction.setEnabled(true);
        }
    };

    connect(&_statisticsAction, &OptionAction::currentIndexChanged, this, updateStatistics);
    _statisticsAction.setEnabled(true);
    _statisticsAction.setCurrentIndex(0);
    updateStatistics();
}

