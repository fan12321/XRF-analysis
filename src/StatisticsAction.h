#pragma once

#include <actions/GroupAction.h>
#include <actions/OptionAction.h>
#include <actions/DecimalAction.h>
#include <actions/IntegralAction.h>

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
    DecimalAction& getRelativeValueAction() { return _relativeValueAction; };
    DecimalAction& getMeanValueAction() { return _meanValueAction; }
    IntegralAction& getNumberOfPeaksAction() { return _numberOfPeaksAction; }

private:
    XRFAnalysisPlugin*      _XRFAnalysisPlugin;
    OptionAction            _statisticsAction;
    DecimalAction           _relativeValueAction;
    DecimalAction           _meanValueAction;
    IntegralAction          _numberOfPeaksAction;
};