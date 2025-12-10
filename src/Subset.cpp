#include "XRFAnalysisPlugin.h"
#include "Subset.h"

Subset::Subset(QObject* parent, const QString& title, int uniqueID) :
    GroupAction(parent, title), 
    _parent(nullptr), 
    _XRFAnalysisPlugin(nullptr), 
    _visibleAction(this, "Visible", false),
    _colorAction(this, "Color"),
    _nameAction(this, "Name")
{
    id = uniqueID;
    addAction(&_visibleAction);
    addAction(&_colorAction);
    addAction(&_nameAction);

    _nameAction.setEnabled(false);
    _nameAction.setToolTip("Name of the subset");
    _colorAction.setToolTip("Color of the subset");
    _visibleAction.setToolTip("Visible");
}

void Subset::initialize(XRFAnalysisPlugin* plugin) {
    _XRFAnalysisPlugin = plugin;

    connect(&_visibleAction, &ToggleAction::toggled, this, [this](bool toggled) -> void {
        if (_XRFAnalysisPlugin == nullptr)
            return;
        _XRFAnalysisPlugin->convertDataAndUpdateChart();
    });
}

Subset::~Subset() {
    if (_parent == nullptr) return;
    auto siblings = _parent->getChildren();
    for (auto it=siblings.begin(); it!=siblings.end(); it++) {
        if (*it == this) {
            siblings.erase(it);
            return;
        }
    }
}

void Subset::setIndices(std::vector<std::uint32_t>& indices) { 
    _indices = indices; 
    subsetSize = indices.size();
}

void Subset::setHistogramData(std::vector<float>& data) {
    _histogramData = data;
}

void Subset::addChild(Subset* child) {
    child->setParent(this);
    _children.push_back(child);
}