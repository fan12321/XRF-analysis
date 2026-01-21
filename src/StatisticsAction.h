#pragma once

#include <actions/GroupAction.h>
#include <actions/OptionAction.h>
#include <actions/DecimalAction.h>
#include <actions/IntegralAction.h>
#include <actions/ToggleAction.h>

using namespace mv::gui;

class XRFAnalysisPlugin;

class StatisticsAction: public GroupAction {
    Q_OBJECT
public:

    /**
     * Construct with \p parent object and \p title
     * @param parent Pointer to parent object
     * @param title Title
     */
    Q_INVOKABLE StatisticsAction(QObject* parent, const QString& title);

public:
    OptionAction& getStatisticsAction() { return _statisticsAction; }
    ToggleAction& getUseQuantileAction() { return _useQuantileAction; };
    DecimalAction& getMeanValueAction() { return _meanValueAction; }
    IntegralAction& getNumberOfPeaksAction() { return _numberOfPeaksAction; }
    DecimalAction& getLogLikelihoodAction() { return _logLikelihoodAction; };

private:
    XRFAnalysisPlugin*      _XRFAnalysisPlugin;
    OptionAction            _statisticsAction;
    ToggleAction            _useQuantileAction;
    DecimalAction           _meanValueAction;
    IntegralAction          _numberOfPeaksAction;
    DecimalAction           _logLikelihoodAction;
};