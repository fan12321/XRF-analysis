#pragma once

#include <actions/GroupAction.h>
#include <actions/ToggleAction.h>
#include <actions/StringAction.h>
#include <actions/ColorAction.h>

class XRFAnalysisPlugin;

using namespace mv::gui;

class Subset : public GroupAction
{
    Q_OBJECT
public:
    Q_INVOKABLE Subset(QObject* parent, const QString& title, int uniqueID = 1);
    virtual ~Subset();
    void initialize(XRFAnalysisPlugin* plugin);
    void setColor(QColor color) { _colorAction.setColor(color); };
    void setName(QString name) { _nameAction.setString(name); };
    void setVisibility(bool check) { _visibleAction.setChecked(check); };
    void setParent(Subset* parent) { _parent = parent; };
    Subset* getParent() { return _parent; };

    void setIndices(std::vector<std::uint32_t>&);
    void setHistogramData(std::vector<float>&);
    void addChild(Subset*);
    
    std::vector<std::uint32_t>& getIndices() { return _indices; };
    std::vector<float>& getHistogramData() { return _histogramData; };
    std::vector<Subset*>& getChildren() { return _children; };
    QString getName() { return _nameAction.getString(); };

    ToggleAction& getVisibleAction() { return _visibleAction; }
    ColorAction& getColorAction() { return _colorAction; }
    StringAction& getNameAction() { return _nameAction; }
    int getSubsetSize() { return subsetSize; };
    int getId() { return id; };
    int numberOfChildren() { return _children.size(); };

    int getChannelId() { return _channelId; };
    float getMean() { return _mean; };
    float getStd() { return _std; };
    float getWeight() { return _weight; };
    void setChannelId(int channelId) { _channelId = channelId; };
    void setMean(float mean) { _mean = mean; };
    void setStd(float std) { _std = std; };
    void setWeight(float weight) { _weight = weight; };
    
private:
    XRFAnalysisPlugin* _XRFAnalysisPlugin;
    ToggleAction    _visibleAction;
    ColorAction     _colorAction;
    StringAction    _nameAction;

    std::vector<std::uint32_t> _indices;
    std::vector<float> _histogramData;
    std::vector<Subset*> _children;
    Subset* _parent;

    int subsetSize{0};
    int id; // subset id

    int _channelId {-1};
    float _mean;
    float _std;
    float _weight;
};