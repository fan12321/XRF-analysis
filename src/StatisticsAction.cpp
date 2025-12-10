#include "StatisticsAction.h"
#include "XRFAnalysisPlugin.h"

using namespace mv::gui;

StatisticsAction::StatisticsAction(QObject* parent, const QString& title) : 
    GroupAction(parent, title), 
    _XRFAnalysisPlugin(dynamic_cast<XRFAnalysisPlugin*>(parent)), 
    _statisticsAction(this, "Statistics", {"Relative value", "Mean value", "Number of peaks"}, "Relative value"), 
    _relativeValueAction(this, "Relative", 0.0, 1.0, 0.5, 2), 
    _meanValueAction(this, "Mean", 0.0, 1.0, 0.5, 2), 
    _numberOfPeaksAction(this, "Peaks", 1, 5, 2)
{
    setIconByName("arrow-down-wide-short");
    setLabelSizingType(LabelSizingType::Auto);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::HiddenInActionContextMenu);
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);

    addAction(&_statisticsAction);
    addAction(&_relativeValueAction);
    addAction(&_meanValueAction);
    addAction(&_numberOfPeaksAction);

    auto const updateStatistics = [this]() -> void {
        int id = _statisticsAction.getCurrentIndex();
        if (id == 0) {
            _relativeValueAction.setEnabled(true);
            _meanValueAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(false);
        }
        else if (id == 1) {
            _relativeValueAction.setEnabled(false);
            _meanValueAction.setEnabled(true);
            _numberOfPeaksAction.setEnabled(false);
        }
        else if (id == 2) {
            _relativeValueAction.setEnabled(false);
            _meanValueAction.setEnabled(false);
            _numberOfPeaksAction.setEnabled(true);
        }
    };

    connect(&_statisticsAction, &OptionAction::currentIndexChanged, this, updateStatistics);
    _statisticsAction.setEnabled(true);
    _statisticsAction.setCurrentIndex(0);
    updateStatistics();
}

