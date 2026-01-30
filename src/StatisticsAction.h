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
    OptionAction& getStatisticsAction() { return _statisticsAction; };
    DecimalAction& getRelativeMeanTreeAction() { return _relativeMeanTreeAction; };
    DecimalAction& getMeanValueAction() { return _meanValueAction; };
    DecimalAction& getRelativeMeanValueAction() { return _relativeMeanValueAction; };
    DecimalAction& getVarianceAction() { return _varianceAction; };
    DecimalAction& getMADAction() { return _madAction; };
    IntegralAction& getNumberOfPeaksAction() { return _numberOfPeaksAction; };
    DecimalAction& getLogLikelihoodAction() { return _logLikelihoodAction; };

private:
    XRFAnalysisPlugin*      _XRFAnalysisPlugin;
    DecimalAction           _relativeMeanTreeAction;
    OptionAction            _statisticsAction;
    DecimalAction           _meanValueAction;
    DecimalAction           _relativeMeanValueAction;
    DecimalAction           _varianceAction;
    DecimalAction           _madAction;
    IntegralAction          _numberOfPeaksAction;
    DecimalAction           _logLikelihoodAction;
};