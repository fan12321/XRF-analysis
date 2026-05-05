#include "XRFAnalysisPlugin.h"
#include "Subset.h"

Subset::Subset(QObject* parent, const QString& title, int uniqueID) :
    GroupAction(parent, title), 
    _parent(nullptr), 
    _XRFAnalysisPlugin(nullptr), 
    _visibleAction(this, "Visible", true),
    _colorAction(this, "Color"),
    _nameAction(this, "Name"), 
    _clusters(nullptr)
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
}

Subset::~Subset() {
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

Subset* Subset::getRoot() {
    Subset* root = this;
    while (root->getParent() != nullptr) root = root->getParent();
    
    return root;
}