// ManiVault invokes this function to set the plot data,
// when emitting qt_js_setDataInJS from the communication object
// The connection is established in qwebchannel.tools.js
data_format = [
    {
        "subset": "selection",
        "id": 0,
        "values": [
            {
                "element": "Hg",
                "value": 30
            },
            {
                "element": "Pb",
                "value": 70
            },
            {
                "element": "Fe",
                "value": 50
            },
            {
                "element": "Cu",
                "value": 90
            }
        ]
    },
    {
        "subset": "background",
        "id": 1,
        "values": [
            {
                "element": "Hg",
                "value": 10
            },
            {
                "element": "K",
                "value": 20
            },
            {
                "element": "Fe",
                "value": 15
            },
            {
                "element": "Cu",
                "value": 25
            }
        ]
    }
];

splits_dataformat = [
    {
        "element": "A",
        "values": [
            {
                "split": "c1",
                "value": .30,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            },
            {
                "split": "c2",
                "value": .10,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            }
        ]
    },
    {
        "element": "B",
        "values": [
            {
                "split": "c1",
                "value": .70,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            },
            {
                "split": "c2",
                "value": .20,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            }
        ]
    },
    {
        "element": "C",
        "values": [
            {
                "split": "c1",
                "value": .50,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            },
            {
                "split": "c2",
                "value": .15,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            }
        ]
    },
    {
        "element": "D",
        "values": [
            {
                "split": "c1",
                "value": .90,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            },
            {
                "split": "c2",
                "value": .55,
                "q0": 0.3,
                "q1": 0.5,
                "q2": 0.7,
                "q3": 0.8,
                "q4": 0.95
            }
        ]
    },
]

splits = [

];

treeData = {

};

showingPreviousContent = false;
previousTooltipContent = null;
tooltipContentMemory = {};

channels = [];
var pinnedChannels = new Set();

line = [];
line_bg = [];
histogram_min = 0;
histogram_max = 0;

var previousData = null;
var statisticType = 0; // 0: intensity, 1: peaks
var colorDomains = [
    [0.0, 1.0],         // mean
    [-0.3, 0.3],        // relative mean
    [0.0, 0.03],         // variance
    [0.0, 0.3],         // median absolute deviation
    [1, 5],             // peaks
    [-20.0, 20.0]       // log likelihood
];
var threshold = 0.5;

var tooltipPinned = false;
var thresholds = [];

// Gaussians
var means = [];
var sds = [];
var weights = [];

function setChannels(data) {
    channels = data;
}

function setMatchingElements(data) {
    if (data && data.length > 0) {
        pinnedChannels = new Set(data);
    }
    else pinnedChannels = new Set();
}

function setThreshold(val) {
    statisticType = val[0];
    threshold = val[1];
    drawChart(previousData);
}

function setHistograms(histograms) {
    showingPreviousContent = false;
    line = histograms["selection"];
    line_bg = histograms["full"];
    histogram_min = histograms["min"];
    histogram_max = histograms["max"];
}

function setGaussians(gaussians) {
    if (gaussians == null) return;
    if (gaussians["means"] == null) return;
    means = gaussians["means"];
    sds = gaussians["sigmas"];
    weights = gaussians["weights"];
}

// -2: no splits
// -1: comparing clusters
// >= 0: comparing subsets
var comparingSplitsSrcId = -2;
function setSplits(splitsData) {
    splits = (comparingSplitsSrcId >= -1) ? splitsData : [];
    drawChart(previousData);
}

function setPreviewSplits(splitsData) {
    comparingSplitsSrcId = -1;
    splits = splitsData;
}

function locateNewParent(tree) {
    if (tree["newParent"]) {
        tooltipContentMemory[tree["id"]] = previousTooltipContent;
        return;
    }
    else {
        for (var child of tree["children"] || []) {
            locateNewParent(child);
        }
    }
}

function setTreeData(tree) {
    locateNewParent(tree);
    treeData = tree;
    drawChart(previousData);
}

var focusNodeId = null;
function setFocusNodeId(nodeId) {
    focusNodeId = nodeId;
}

var previousElement, previousValue, previousColors;
function updateTooltipContent(element, value) {
    previousElement = element;
    previousValue = value;
    // info text (sparkline will be drawn in the popup below)

    // create/remove popup used for the line plot (and split controls)
    d3.select("#splits-popup").remove();
    var popup = d3.select("body").append("div").attr("id", "splits-popup")
        .style("position", "fixed")
        .style("left", "12px")
        .style("right", "12px")
        .style("bottom", "12px")
        .style("height", "150px")
        .style("background-color", "white")
        .style("border", "1px solid #ccc")
        .style("box-shadow", "0 2px 5px rgba(0,0,0,0.12)")
        .style("z-index", "10001")
        .style("overflow", "auto")
        .style("padding", "8px")
        .style("border-radius", "8px 8px 0 0");
    // add a container inside popup for drawing the sparkline/line plot
    popup
        .html("Channel: " + element + (value == null ? "<br>(no value)" : "<br>Value: " + value));

    // render a simple HTML button with border instead of SVG rect
    if (!showingPreviousContent) popup.append("button")
        .attr("id", "split-button")
        .text("Split")
        .style("position", "absolute")
        .style("top", "15px")
        .style("right", "15px")
        .style("width", "60px")
        .style("height", "28px")
        .style("font-size", "10px")
        .style("padding", "2px 6px")
        .style("border", "1px solid #ccc")
        .style("border-radius", "4px")
        .style("background-color", "#f7f7f7")
        .style("cursor", "pointer");

    // add button click handler
    popup.select("#split-button").on("click", function () {
        previousTooltipContent = {
            "element": element, 
            "value": value, 
            "cuts": thresholds.sort(), 
            "line": line, 
            "line_bg": line_bg, 
            "min": histogram_min,
            "max": histogram_max
        }
        passSplitCutsToQt({
            "element": element,
            "cuts": thresholds.sort()
        });
    });
    popup.append("div").attr("class", "plot-container").style("position", "relative");

    // draw sparkline/line plot from global `line` array (no-op if missing)
    if (typeof line === 'undefined' || !Array.isArray(line) || line.length === 0) return;
    // switch drawing target from tooltip to popup
    var drawTarget = popup.select(".plot-container");

    var dataMain = line;
    var dataBg = (typeof line_bg !== 'undefined' && Array.isArray(line_bg)) ? line_bg : null;
    var sparkW = popup.node().getBoundingClientRect().width * 0.8;
    var sparkH = 90;
    var marginS = { left: 32, right: 10, top: 10, bottom: 36 };
    var innerW = sparkW - marginS.left - marginS.right;
    var innerH = sparkH - marginS.top - marginS.bottom;

    var nMain = dataMain.length;
    var nBg = dataBg ? dataBg.length : 0;
    var nMax = Math.max(nMain, nBg);

    var combined = dataMain.slice();
    if (dataBg) combined = combined.concat(dataBg);
    var minv = d3.min(combined), maxv = d3.max(combined);
    if (minv === maxv) { minv = maxv - 1; } // avoid zero-range

    // var xS = d3.scaleLinear().domain([0, Math.max(1, nMax - 1)]).range([marginS.left, marginS.left + innerW]);
    var xS = d3.scaleLinear().domain([histogram_min, histogram_max]).range([marginS.left, marginS.left + innerW]);
    var yS = d3.scaleLinear().domain([minv, maxv]).range([marginS.top + innerH, marginS.top]);

    var lineGen = d3.line()
        // .x(function (_, i) { return xS(i); })
        .x(function (_, i) { return xS(histogram_min + (i / (nMax - 1)) * (histogram_max - histogram_min)); })
        .y(function (v) { return yS(v); })
        .curve(d3.curveMonotoneX);

    var areaGen = d3.area()
        // .x(function (_, i) { return xS(i); })
        // .x(function (_, i) { return xS(i / (nMax - 1)); })
        .x(function (_, i) { return xS(histogram_min + (i / (nMax - 1)) * (histogram_max - histogram_min)); })
        .y0(yS(minv)) // baseline for the filled area
        .y1(function (v) { return yS(v); })
        .curve(d3.curveMonotoneX);

    // create/update svg inside popup
    var svg = drawTarget.selectAll("svg.sparkline").data([dataMain]);
    var svgEnter = svg.enter().append("svg")
        .attr("class", "sparkline")
        .attr("width", sparkW)
        .attr("height", sparkH);
    // axis groups and containers on enter
    svgEnter.append("g").attr("class", "x axis");
    svgEnter.append("g").attr("class", "y axis");
    svgEnter.append("g").attr("class", "paths");
    svgEnter.append("g").attr("class", "threshold");
    svg = svgEnter.merge(svg);

    // update axes
    // var xAxis = d3.axisBottom(xS).ticks(4).tickSize(3).tickFormat(function (d) { return d === 0 || d === nMax - 1 ? d : ''; });
    var xAxis = d3.axisBottom(xS).ticks(5).tickSize(3).tickFormat(function (d) { return d; });
    var yAxis = d3.axisLeft(yS).ticks(3).tickSize(3);
    svg.select("g.x.axis")
        .attr("transform", "translate(0," + (marginS.top + innerH) + ")")
        .call(xAxis)
        .selectAll("text").style("font-size", "9px");
    svg.select("g.y.axis")
        .attr("transform", "translate(" + marginS.left + ",0)")
        .call(yAxis)
        .selectAll("text").style("font-size", "9px");

    if (dataBg) {
        var bgArea = svg.select("g.paths").selectAll("path.bgarea").data([dataBg]);
        bgArea.enter().append("path").attr("class", "bgarea")
            .merge(bgArea)
            .attr("d", areaGen)
            .attr("fill", "rgba(154, 154, 154, 0.5)");

        var bgLine = svg.select("g.paths").selectAll("path.bgline").data([dataBg]);
        bgLine.enter().append("path").attr("class", "bgline")
            .merge(bgLine)
            .attr("d", lineGen)
            .attr("fill", "none")
            .attr("stroke", "rgb(154, 154, 154)")
            .attr("stroke-width", 1.2);
    } else {
        svg.select("g.paths").selectAll("path.bgarea").remove();
        svg.select("g.paths").selectAll("path.bgline").remove();
    }

    // main area + stroke line
    var mainArea = svg.select("g.paths").selectAll("path.mainarea").data([dataMain]);
    mainArea.enter().append("path").attr("class", "mainarea")
        .merge(mainArea)
        .attr("d", areaGen)
        .attr("fill", "rgba(154, 51, 77, 0.5)");

    var mainLine = svg.select("g.paths").selectAll("path.sparkpath").data([dataMain]);
    mainLine.enter().append("path").attr("class", "sparkpath")
        .merge(mainLine)
        .attr("d", lineGen)
        .attr("fill", "none")
        .attr("stroke", "rgb(154, 51, 77)")
        .attr("stroke-width", 1.6);

    // compute multiple gaussians on the same x-grid (0 .. nMax-1)
    var gaussSum = new Array(nMax).fill(0);
    var palette =  d3.schemeCategory10 || ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"];
    var histogramSum = d3.sum(dataMain);
    for (var j = 0; j < Math.max(means.length, sds.length, weights.length); j++) {
        var mu = (means[j] !== undefined) ? means[j] : 0;
        var sigma = (sds[j] !== undefined) ? sds[j] : 1;
        var w = (weights[j] !== undefined) ? weights[j] : 1.0;
        // build gaussian (peak=1)
        var ga = [];
        for (var i = 0; i < nMax; i++) {
            var z = (i - mu) / sigma;
            var gv = Math.exp(-0.5 * z * z) / (sigma * Math.sqrt(2 * Math.PI));
            ga.push(gv);
            gaussSum[i] += w * gv * histogramSum;
        }
        // scale gaussian into data y-range by its weight (visual scaling)
        var gaussScaled = ga.map(function (gv) {
            return w * gv * histogramSum;
        });

        // draw each gaussian line (distinct color)
        (function (idx, arr) {
            var color = palette[idx % palette.length];
            var cls = "gauss_" + idx;
            var gpath = svg.select("g.paths").selectAll("path." + cls).data([arr]);
            gpath.enter().append("path").attr("class", cls)
                .merge(gpath)
                .attr("d", lineGen)
                .attr("fill", "none")
                .attr("stroke", color)
                .attr("stroke-width", 2.5);
            // .attr("stroke-dasharray", "4,3");
        })(j, gaussScaled);

    }

    svg.on("click", function (event) {
        if (showingPreviousContent) return;
        event.stopPropagation();
        if (!tooltipPinned) return;
        var [x, y] = d3.pointer(event, this);
        var i = xS.invert(x);
        if (thresholds.length < 3) {
            thresholds.push(i);
            drawThresholds(svg, thresholds, xS, yS, minv, maxv, element);
            // passCompareClustersSignalToQt();
        }
    });

    function findNearest(arr, val) {
        if (arr.length === 0) return -1;
        var minDist = Infinity, index = -1;
        arr.forEach(function (t, i) {
            var dist = Math.abs(t - val);
            if (dist < minDist) {
                minDist = dist;
                index = i;
            }
        });
        return index;
    }

    svg.on("contextmenu", function (event) {
        if (showingPreviousContent) return;
        event.preventDefault();
        event.stopPropagation();
        var [x] = d3.pointer(event, this);
        var clickedX = xS.invert(x);
        var nearestIndex = findNearest(thresholds, clickedX);
        if (nearestIndex !== -1) {
            thresholds.splice(nearestIndex, 1);
            drawThresholds(svg, thresholds, xS, yS, minv, maxv, element);
        }
    });
    if (thresholds.length > 0) drawThresholds(svg, thresholds, xS, yS, minv, maxv, element);
}


function drawChart(data) {
    if (data != null && data.length < 1) {
        log("XRF Analysis Plugin: radar_chart.tools.js: data empty")
        return
    }

    previousData = data;

    d3.select("div#container").selectAll("div").remove();
    d3.select("div#container").selectAll("svg").remove();

    var margin = { top: 20, right: 25, bottom: 20, left: 80 },
        width = window.innerWidth - margin.left - margin.right;

    // layout params for tiles
    var tileW = 90;            // preferred tile width
    var tileH = 40;             // tile height
    var gapX = 12;              // horizontal gap between tiles
    var gapY = 8;               // vertical gap between tile rows (within a subset)
    var subsetLabelPad = 12;    // left label width reserve

    // compute per-subset required heights and overall height using filtered values
    var subsetLayout = data.map(function (sub) {
        var vals = sub.values || [];
        // support per-subset flag stored on the data object

        // Separate pinned and unpinned channels (before threshold filtering)
        var allPinned = vals.filter(function (v) { return pinnedChannels.has(v.element); });
        var allUnpinned = vals.filter(function (v) { return !pinnedChannels.has(v.element); });

        // Apply threshold filter only to unpinned channels; pinned channels always show
        var pinned = allPinned.sort(function (a, b) { return b.value - a.value; });
        var unpinned = (!sub.filterLow ? allUnpinned.filter(function (v) { return (v.value || 0) >= threshold; }) : allUnpinned)
            .sort(function (a, b) { return b.value - a.value; });

        // Combine: pinned first, then unpinned
        var filtered = pinned.concat(unpinned);
        var n = filtered.length;

        // how many tiles fit per visual row for the available width
        var perRow = Math.max(1, Math.floor((width + gapX) / (tileW + gapX)));
        if (perRow > 1) perRow -= 1;
        // adjust tileW down if too wide for computed perRow (distribute remaining space)
        var computedTileW = Math.min(tileW, Math.floor((width - (perRow - 1) * gapX) / perRow));

        // Calculate heights for pinned and unpinned sections separately
        var pinnedCount = pinned.length;
        var unpinnedCount = unpinned.length;
        var pinnedRowsNeeded = Math.max(pinnedCount > 0 ? 1 : 0, Math.ceil(pinnedCount / perRow));
        var unpinnedRowsNeeded = Math.max(unpinnedCount > 0 ? 1 : 0, Math.ceil(unpinnedCount / perRow));

        var pinnedHeight = pinnedRowsNeeded > 0 ? pinnedRowsNeeded * tileH + Math.max(0, pinnedRowsNeeded - 1) * gapY : 0;
        var unpinnedHeight = unpinnedRowsNeeded > 0 ? unpinnedRowsNeeded * tileH + Math.max(0, unpinnedRowsNeeded - 1) * gapY : 0;
        var separatorHeight = (pinnedCount > 0 && unpinnedCount > 0) ? 12 : 0; // height for separator line

        var rowsNeeded = pinnedRowsNeeded + unpinnedRowsNeeded;
        var innerHeight = pinnedHeight + unpinnedHeight + separatorHeight + gapY;

        return {
            subset: sub.subset,
            n: n,
            perRow: perRow,
            tileW: computedTileW,
            tileH: tileH,
            rowsNeeded: rowsNeeded,
            innerHeight: innerHeight,
            filtered: filtered,
            pinnedCount: pinnedCount,
            unpinnedCount: unpinnedCount,
            pinnedHeight: pinnedHeight,
            unpinnedHeight: unpinnedHeight,
            separatorHeight: separatorHeight
        };
    });

    var innerHeight = subsetLayout.reduce(function (acc, s) { return acc + s.innerHeight + 8; }, 0); // +8 padding between subsets
    var svgHeight = innerHeight + margin.top + margin.bottom;

    var svg = d3.select("div#container")
        .append("svg")
        .attr("width", width + margin.left + margin.right)
        .attr("height", svgHeight)
        .append("g")
        .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

    // color scale
    var myColor = d3.scaleSequential()
        .interpolator((statisticType == 1) ? d3.interpolateRdBu : d3.interpolateBlues)
        .domain(colorDomains[statisticType]);


    var mouseover = function (event, d) {
        event.stopPropagation();
        if (tooltipPinned) return;
        var subsetName = d3.select(this.parentNode.parentNode).datum().subset;
        var subsetId = d3.select(this.parentNode.parentNode).datum().id;
        var element = d.element;
        d3.select(this).style("stroke", "#333").style("opacity", 1);

        passFocusingElementToQt({
            "subset": subsetName,
            "element": element,
            "subsetId": subsetId
        });
    };
    var mousemove = function (event, d) {
        if (tooltipPinned) return;
        // ensure datum is available
        if (d === undefined) d = d3.select(this).datum();

        var subsetName = d3.select(this.parentNode.parentNode).datum().subset;
        var subsetId = d3.select(this.parentNode.parentNode).datum().id;
        var element = d.element;

        passFocusingElementToQt({
            "subset": subsetName,
            "element": element,
            "subsetId": subsetId
        });

        updateTooltipContent(d.element, d.value);
    };


    var mouseleave = function (event, d) {
        event.stopPropagation();
        if (tooltipPinned) return;
        d3.select(this).style("stroke", "none").style("opacity", 0.95);
        // clear thresholds
        thresholds = [];
        d3.select("#splits-popup").selectAll("svg.sparkline g.threshold *").remove();
    };


    // create groups for subsets and place them vertically with computed heights
    var yCursor = 0;
    data.forEach(function (sub, i) {
        var layout = subsetLayout[i];
        var g = svg.append("g")
            .attr("class", "subset")
            .attr("transform", "translate(0," + yCursor + ")")
            .datum(sub);

        // subset label on left
        g.append("text")
            .attr("x", -subsetLabelPad)
            .attr("y", layout.tileH / 2)
            .attr("dy", "0.35em")
            .attr("text-anchor", "end")
            .style("font-size", "12px")
            .style("fill", "#333")
            .text(sub.subset);

        // toggle button (small rect + text) placed at right edge of the subset block
        var toggleW = 45, toggleH = 24, togglePad = 6;
        var toggleX = width - 1.5 * toggleW;
        var toggleY = toggleH / 2; // slightly above the top of the subset area
        var toggle = g.append("g")
            .attr("class", "toggle")
            .attr("transform", "translate(" + toggleX + "," + toggleY + ")")
            .style("cursor", "pointer")
            .on("click", function () {
                sub.filterLow = !sub.filterLow; // flip filter flag on underlying data
                drawChart(data); // re-render full chart so layout recomputes
            });

        toggle.append("rect")
            .attr("width", toggleW)
            .attr("height", toggleH)
            .attr("rx", 4)
            .attr("ry", 4)
            .style("fill", "#f7f7f7")
            .style("stroke", "#ccc");

        toggle.append("text")
            .attr("x", toggleW / 2)
            .attr("y", toggleH / 2)
            .attr("dy", "0.35em")
            .attr("text-anchor", "middle")
            .style("font-size", "11px")
            .style("fill", "#333")
            .html(sub.filterLow ? "&#9652;" : "&#9662;");

        // add tiles using filtered list, wrap to next line when exceeding perRow
        layout.filtered.forEach(function (v, idx) {
            // Determine if this item is pinned
            var isPinned = pinnedChannels.has(v.element);

            // Calculate position within each section (pinned or unpinned)
            var position;
            if (isPinned) {
                // Position within pinned section
                position = idx;
            } else {
                // Position within unpinned section (starting from 0)
                position = idx - layout.pinnedCount;
            }

            var col = position % layout.perRow;
            var row = Math.floor(position / layout.perRow);

            // Offset y position for unpinned tiles
            var yOffset = 0;
            if (!isPinned && layout.pinnedCount > 0) {
                yOffset = layout.pinnedHeight + layout.separatorHeight;
            }

            var x = col * (layout.tileW + gapX);
            var y = yOffset + row * (layout.tileH + gapY);

            var cell = g.append("g")
                .attr("class", "cell")
                .attr("transform", "translate(" + x + "," + y + ")")
                .datum(v);

            cell.append("rect")
                .attr("id", "tile-" + v.element)
                .attr("class", "-tile-rect")
                .attr("x", 0)
                .attr("y", 0)
                .attr("width", layout.tileW)
                .attr("height", layout.tileH)
                .attr("rx", 6)
                .attr("ry", 6)
                .style("fill", function () { return myColor(v.value); })
                .style("opacity", 0.95)
                .on("mouseover", mouseover)
                .on("mousemove", mousemove)
                .on("mouseleave", mouseleave)
                .on("click", function (event, d) {
                    if (tooltipPinned) {
                        event.stopPropagation();
                        var subsetName = d3.select(this.parentNode.parentNode).datum().subset;
                        var subsetId = d3.select(this.parentNode.parentNode).datum().id;
                        var element = d.element;
                        d3.select(this).style("stroke", "#333").style("opacity", 1);

                        passFocusingElementToQt({
                            "subset": subsetName,
                            "element": element,
                            "subsetId": subsetId
                        });
                        tooltipPinned = false;
                        d3.selectAll("rect.-tile-rect").style("stroke", "none").style("opacity", 0.95);
                        updateTooltipContent(d.element, d.value);
                        return;
                    }
                    event.stopPropagation();
                    tooltipPinned = true;
                    d3.selectAll("rect.-tile-rect").style("stroke", "none").style("opacity", 0.95);
                    d3.select(this).style("stroke", "#333").style("opacity", 1).style("stroke-width", 3);
                    updateTooltipContent(d.element, d.value);
                });

            cell.on("dblclick", function (event, d) {
                var dblClickData = d3.select(this.parentNode).datum();
                // passDblClickSplitToQt({
                //     "subset": dblClickData.subset,
                //     "element": d.element,
                //     "subsetId": dblClickData.id
                // });
                // drawChart(data);
            });

            var valueForTextColorChange = (colorDomains[statisticType][1] - colorDomains[statisticType][0]) * 0.8 + colorDomains[statisticType][0];
            cell.append("text")
                .attr("x", layout.tileW / 2)
                .attr("y", layout.tileH / 2)
                .attr("dy", "0.35em")
                .attr("text-anchor", "middle")
                .style("fill", (v.value < valueForTextColorChange) ? "#111" : "#fff")
                .style("font-weight", "600")
                .style("pointer-events", "none")
                .text(v.element);
        });

        // Draw separator line between pinned and unpinned sections
        if (layout.pinnedCount > 0 && layout.unpinnedCount > 0) {
            g.append("line")
                .attr("x1", -9999)
                .attr("x2", 9999)
                .attr("y1", layout.pinnedHeight + layout.separatorHeight / 2)
                .attr("y2", layout.pinnedHeight + layout.separatorHeight / 2)
                .style("stroke", "#ddd")
                .style("stroke-width", 2)
                .style("stroke-dasharray", "4,4");
        }

        // separator line after subset block
        svg.append("line")
            .attr("x1", -9999)
            .attr("x2", 9999)
            .attr("y1", yCursor + layout.innerHeight + 4)
            .attr("y2", yCursor + layout.innerHeight + 4)
            .style("stroke", "#eee")
            .style("stroke-width", 1);

        yCursor += layout.innerHeight + 8;
    });

    function renderTree() {
        if (typeof treeData === 'undefined') return;

        // remove previous tree if any
        d3.select("div#container").selectAll("svg.tree-svg").remove();

        var treeHeight = 280;
        var treeMargin = { top: 20, right: 60, bottom: 20, left: 20 };
        var treeWidth = Math.max(300, width + margin.left + margin.right); // match chart width-ish

        var treeSvg = d3.select("div#container")
            .append("svg")
            .attr("class", "tree-svg")
            .attr("width", treeWidth)
            .attr("height", treeHeight)
            .style("display", "block")
            .style("margin-top", "12px");

        // group inside svg with left margin for labels if needed
        var g = treeSvg.append("g")
            .attr("transform", "translate(" + (margin.left) + "," + (treeMargin.top) + ")");

        // layout: horizontal tree (left -> right)
        var innerW = Math.max(100, (treeWidth - margin.left - margin.right - treeMargin.left - treeMargin.right));
        var innerH = Math.max(60, treeHeight - treeMargin.top - treeMargin.bottom);

        var root = d3.hierarchy(treeData);
        var treeLayout = d3.tree().size([innerH, innerW]);
        treeLayout(root);

        // links (use horizontal link generator)
        var linkGen = d3.linkHorizontal()
            .x(function (d) { return d.y; })
            .y(function (d) { return d.x; });

        var links = g.selectAll(".tree-link")
            .data(root.links())
            .enter()
            .append("path")
            .attr("class", "tree-link")
            .attr("d", function (d) { return linkGen({ source: d.source, target: d.target }); })
            .attr("fill", "none")
            .attr("stroke", function (d) {
                var srcId = d && d.source && d.source.data && d.source.data.id;
                return srcId === comparingSplitsSrcId ? "#777" : "#bbb";
            })
            .attr("stroke-width", function (d) {
                var srcId = d && d.source && d.source.data && d.source.data.id;
                return srcId === comparingSplitsSrcId ? 5 : 4;
            })
            .style("cursor", "pointer")
            .style("pointer-events", "stroke")
            .on("click", function (event, d) {
                var srcId = d && d.source && d.source.data ? d.source.data.id : null;
                if (comparingSplitsSrcId == srcId) comparingSplitsSrcId = -2;
                else comparingSplitsSrcId = srcId;

                line = tooltipContentMemory[srcId]["line"];
                line_bg = tooltipContentMemory[srcId]["line_bg"];
                thresholds = tooltipContentMemory[srcId]["cuts"] || [];
                previousElement = tooltipContentMemory[srcId]["element"];
                previousValue = tooltipContentMemory[srcId]["value"];
                var parentNode = root.descendants().find(function(node) { return node.data.id === srcId; });
                previousColors = (parentNode && parentNode.children) ? parentNode.children.map(function(child) { return child.data.color; }) : [];
                event.stopPropagation();
                showingPreviousContent = true;
                updateTooltipContent(previousElement, previousValue);

                renderTree();
                // passParentNodeIdToQt({
                //     "id": srcId
                // });
            });

        // render global splits popup fixed at bottom of viewport
        var nodes = g.selectAll(".tree-node")
            .data(root.descendants())
            .enter()
            .append("g")
            .attr("class", "tree-node")
            .attr("transform", function (d) { return "translate(" + d.y + "," + d.x + ")"; })
            .style("cursor", "pointer")
            .on("click", function (event, d) {
                // visual highlight
                g.selectAll(".tree-node circle").attr("stroke", "#fff").attr("stroke-width", 2);
                // d3.select(this).select("circle").attr("stroke", "#333").attr("stroke-width", 2);

                passNodeIdToQt({
                    "id": d.data.id
                });
            });


        nodes.append("circle")
            .attr("r", 9)
            .attr("fill", function (d) {
                return d.data && d.data.color ? d.data.color : "#fff";
            })
            .attr("stroke", function (d) {
                if (d.data && d.data.id == focusNodeId) {
                    return "#333";
                }
                return "#fff";
            })
            .attr("stroke-width", 3.0);

        nodes.append("text")
            .attr("dx", 0)
            .attr("dy", -15)
            .attr("text-anchor", "middle")
            .style("font-size", "12px")
            .text(function (d) { return d.data.name || ""; });

        // high concentrations: render small circles with up-to-3-char labels to the right of each node
        nodes.each(function (d) {
            var el = d3.select(this);
            var badges = (d.data && Array.isArray(d.data.highConcentrations)) ? d.data.highConcentrations : [];
            var numBadges = badges.length;
            var startX = -16 * (numBadges - 1);
            var spacing = 32;
            var maxLabelLen = 3;

            var badgeSel = el.selectAll("g.badge").data(badges);
            var badgeEnter = badgeSel.enter().append("g").attr("class", "badge")
                .attr("transform", function (b, i) { return "translate(" + (startX + i * spacing) + ",0)"; })
                .style("cursor", "default");

            badgeEnter.append("circle")
                .attr("r", 12)
                .attr("fill", "#f5f5f5")
                .attr("stroke", "#999")
                .attr("stroke-width", 1);

            badgeEnter.append("text")
                .attr("text-anchor", "middle")
                .attr("dy", "0.35em")
                .style("font-size", "10px")
                .style("fill", "#222")
                .text(function (b) { return (b == null ? "" : b.toString().substring(0, maxLabelLen)); });

            // update + exit handling
            badgeSel.merge(badgeEnter)
                .attr("transform", function (b, i) { return "translate(" + (startX + i * spacing) + ",24)"; });
            badgeSel.exit().remove();
        });

        // optional: collapse/expand on double-click (simple toggle)
        nodes.on("dblclick", function (event, d) {
            if (!d.children && !d._children) return;
            if (d.children) {
                d._children = d.children;
                d.children = null;
            } else {
                d.children = d._children;
                d._children = null;
            }
            // redraw small tree by re-calling this function
            // remove and re-render
            d3.select("div#container").selectAll("svg.tree-svg").remove();
            renderTree(); // recursion will re-build with updated hierarchy state
        });

        // right-click on root node to show channel toggle menu
        nodes.on("contextmenu", function (event, d) {
            event.preventDefault();

            // check if this is root node (root node has no parent)
            if (d.parent !== null) return;

            // remove previous menu if any
            d3.selectAll("div.channel-context-menu").remove();

            // create context menu with two columns
            var menu = d3.select("body")
                .append("div")
                .attr("class", "channel-context-menu")
                .style("position", "fixed")
                .style("left", (event.clientX) + "px")
                .style("top", (event.clientY) + "px")
                .style("background-color", "white")
                .style("border", "1px solid #ccc")
                .style("border-radius", "4px")
                .style("box-shadow", "0 2px 8px rgba(0,0,0,0.15)")
                .style("z-index", "10000")
                .style("display", "flex")
                .style("padding", "4px 0");

            // add channel items in two columns
            if (channels && channels.length > 0) {
                // Calculate midpoint for two-column layout
                var mid = Math.ceil(channels.length / 2);

                // Create left and right column containers
                var leftCol = menu.append("div").style("flex", "1");
                var rightCol = menu.append("div").style("flex", "1");

                channels.forEach(function (channel, idx) {
                    var isChecked = pinnedChannels.has(channel);
                    var col = idx < mid ? leftCol : rightCol;

                    var item = col.append("div")
                        .style("padding", "6px 12px")
                        .style("cursor", "pointer")
                        .style("user-select", "none")
                        .style("display", "flex")
                        .style("align-items", "center")
                        .style("gap", "5px")
                        .on("mouseover", function () {
                            d3.select(this).style("background-color", "#f0f0f0");
                        })
                        .on("mouseout", function () {
                            d3.select(this).style("background-color", "transparent");
                        })
                        .on("click", function () {
                            // toggle channel in pinnedChannels set
                            if (pinnedChannels.has(channel)) {
                                pinnedChannels.delete(channel);
                            } else {
                                pinnedChannels.add(channel);
                            }
                            // redraw chart with updated pinned channels
                            drawChart(previousData);
                            // close menu
                            // d3.selectAll("div.channel-context-menu").remove();
                            passMatchingElementsToQt({
                                "elements": Array.from(pinnedChannels).sort()
                            });
                        });

                    // add checkbox
                    var checkbox = item.append("input")
                        .attr("type", "checkbox")
                        .style("margin", "0")
                        .style("cursor", "pointer")
                        .property("checked", isChecked);

                    // add label
                    item.append("span")
                        .text(channel)
                        .style("font-size", "13px");
                });
            } else {
                menu.append("div")
                    .style("padding", "6px 12px")
                    .style("font-size", "13px")
                    .style("color", "#999")
                    .text("No channels available");
            }

            // close menu on click outside
            d3.select("body").on("click", function () {
                d3.selectAll("div.channel-context-menu").remove();
            });
        });
    };
    renderTree();
    d3.select(document).on("click.tooltip", function (event) {
        d3.select("#splits-popup").remove();
        if (tooltipPinned) {
            tooltipPinned = false;
            thresholds = [];
            d3.select("#splits-popup svg.sparkline g.threshold").selectAll("line").remove();

            passNoSplitsSignalToQt();
            comparingSplitsSrcId = -2;
            setSplits(splits);
            renderTree();
        }
    });


    // render a fixed bottom popup that shows the splits x elements table
    // renderSplitsPopup(splits);
}

function drawThresholds(svg, thresholds, xS, yS, minv, maxv, element, end = false) {
    if (!showingPreviousContent) {
        passPreviewSplitsToQt({
            "element": element,
            "cuts": thresholds.sort(),
            "mouseRelease": end
        })
    }
    var threshG = svg.select("g.threshold");
    // clear previous slider elements
    threshG.selectAll("*").remove();

    var lines = threshG.selectAll("line.thresh-line").data(thresholds);
    lines.enter().append("line").attr("class", "thresh-line")
        .merge(lines)
        .attr("x1", function (d) { return xS(d); })
        .attr("x2", function (d) { return xS(d); })
        .attr("y1", yS(minv))
        .attr("y2", yS(maxv))
        .attr("stroke", "red")
        .attr("stroke-width", 2);
    lines.exit().remove();

    // slider visual track placed just below the x axis
    var trackX0 = xS.range()[0];
    var trackX1 = xS.range()[1];
    var trackW = trackX1 - trackX0;
    var trackY = yS(minv) + 20; // a few px below the sparkline baseline
    var trackH = 8;

    // draw colored segments (ranges) divided by handles
    var sortedVals = thresholds.slice().sort(function (a, b) { return a - b; });
    var domain0 = xS.domain()[0];
    var domain1 = xS.domain()[1];
    var boundaries = [domain0].concat(sortedVals).concat([domain1]);

    var segData = boundaries.slice(0, -1).map(function (d, i) { return { x0: boundaries[i], x1: boundaries[i + 1], idx: i }; });

    var palette = (showingPreviousContent)? previousColors : d3.schemeCategory10 || ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"];
    function pickColor(i) { if (i < palette.length) return palette[i]; return "hsl(" + ((i * 55) % 360) + ",60%,65%)"; }

    var segs = threshG.selectAll("rect.range-seg").data(segData, function (d) { return d.idx; });
    segs.enter().append("rect").attr("class", "range-seg")
        .merge(segs)
        .attr("x", function (d) { return xS(d.x0); })
        .attr("y", trackY)
        .attr("width", function (d) { return Math.max(0, xS(d.x1) - xS(d.x0)); })
        .attr("height", trackH)
        .style("fill", function (d) { return pickColor(d.idx); })
        .style("opacity", 0.18)
        .style("stroke", "none");
    segs.exit().remove();

    // draw slider track outline on top of colored segments
    threshG.append("rect")
        .attr("class", "thresh-track")
        .attr("x", trackX0)
        .attr("y", trackY)
        .attr("width", trackW)
        .attr("height", trackH)
        .attr("rx", 4)
        .style("fill", "transparent")
        .style("stroke", "#ddd");

    // data binding uses objects so we can keep index stable during drag
    var dataObjs = thresholds.map(function (t, i) { return { v: t, i: i }; });

    if (showingPreviousContent) return;

    var dragBehaviour = d3.drag()
        .on("start", function (event, d) {
            comparingSplitsSrcId = -1;
            d3.select(this).raise();
        })
        .on("drag", function (event, d) {
            // use pointer relative to the sparkline SVG for accurate positioning
            var pt = d3.pointer(event, svg.node());
            var px = Math.max(trackX0, Math.min(trackX1, pt[0]));
            var val = xS.invert(px);
            thresholds[d.i] = val;
            drawThresholds(svg, thresholds, xS, yS, minv, maxv, element);
        })
        .on("end", function (event, d) {
            // passCompareClustersSignalToQt();
            drawThresholds(svg, thresholds, xS, yS, minv, maxv, element, true);
        });

    var handles = threshG.selectAll("g.handle").data(dataObjs, function (d) { return d.i; });
    var enter = handles.enter().append("g").attr("class", "handle");
    enter.append("circle").attr("r", 7).style("fill", "#fff").style("stroke", "rgb(0, 0, 0)").style("stroke-width", 2).call(dragBehaviour);
    enter.append("text").attr("class", "label").attr("y", 22).attr("text-anchor", "middle").style("font-size", "10px");

    handles = enter.merge(handles);
    handles.attr("transform", function (d) { return "translate(" + xS(d.v) + "," + (trackY + trackH / 2) + ")"; });
    handles.select("text.label").text(function (d) { return Math.round(d.v * 100) / 100; });

    handles.exit().remove();
}

window.onload = function () {
    if (previousData != null) drawChart(previousData);
}

window.onresize = function () {
    drawChart(previousData);
    if (tooltipPinned) updateTooltipContent(previousElement, previousValue);
}