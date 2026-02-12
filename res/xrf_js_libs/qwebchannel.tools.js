var isQtAvailable = false;

// Here, we establish the connection to Qt
// Any signals that we want to send from ManiVault to
// the JS side have to be connected here
try {
    new QWebChannel(qt.webChannelTransport, function (channel) {
        // Establish connection
        QtBridge = channel.objects.QtBridge;

        // register signals
        QtBridge.qt_js_setChannelInfo.connect(function () {
            setChannels(arguments[0]);
        });

        QtBridge.qt_js_setDataAndPlotInJS.connect(function () {
            drawChart(arguments[0]); 
        });   // drawChart is defined in xrf.js

        QtBridge.qt_js_setThreshold.connect(function () {
            setThreshold(arguments[0]);
        });

        QtBridge.qt_js_setHistograms.connect(function () {
            setHistograms(arguments[0]);
        });

        QtBridge.qt_js_setGaussians.connect(function () {
            setGaussians(arguments[0]);
        });

        QtBridge.qt_js_setSplits.connect(function () {
            setSplits(arguments[0]);
        });

        QtBridge.qt_js_setPreviewSplits.connect(function () {
            setPreviewSplits(arguments[0]);
        });

        QtBridge.qt_js_setTreeData.connect(function() {
            setTreeData(arguments[0]);
        });

        QtBridge.qt_js_setFocusNodeId.connect(function() {
            setFocusNodeId(arguments[0]);
        });

        QtBridge.qt_js_setMatchingElements.connect(function() {
            setMatchingElements(arguments[0]);
        });

        // confirm successful connection
        isQtAvailable = true;
        notifyBridgeAvailable();
    });
} catch (error) {
	log("XRF Analysis Plugin: qwebchannel: could not connect qt");
}

// The slot js_available is defined by ManiVault's WebWidget and will
// invoke the initWebPage function of our web widget (here, ChartWidget)
function notifyBridgeAvailable() {

    if (isQtAvailable) {
        QtBridge.js_available();
    }
    else {
        log("XRF Analysis Plugin: qwebchannel: QtBridge is not available - something went wrong");
    }

}

// The QtBridge is connected to our WebCommunicationObject (ChartCommObject)
// and we can call all slots defined therein
function passSelectionToQt(dat) {
    if (isQtAvailable) {
        QtBridge.js_qt_passSelectionToQt(dat);
	}
}

function passFocusingElementToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passFocusingElementToQt(data);
    }
}

function passDblClickSplitToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passDblClickSplitToQt(data);
    }
}

function passSplitCutsToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passSplitCutsToQt(data);
    }
}

function passPreviewSplitsToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passPreviewSplitsToQt(data);
    }
}

function passNoSplitsSignalToQt() {
    if (isQtAvailable) {
        QtBridge.js_qt_passNoSplitsSignalToQt();
    }
}

function passNodeIdToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passNodeIdToQt(data);
    }
}

function passParentNodeIdToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passParentNodeIdToQt(data);
    }
}

function passCompareClustersSignalToQt() {
    if (isQtAvailable) {
        QtBridge.js_qt_passCompareClustersSignalToQt();
    }
}

function passMatchingElementsToQt(data) {
    if (isQtAvailable) {
        QtBridge.js_qt_passMatchingElementsToQt(data);
    }
}

// utility function: pipe errors to log
window.onerror = function (msg, url, num) {
    log("XRF Analysis Plugin: qwebchannel: Error: " + msg + "\nURL: " + url + "\nLine: " + num);
};

// utility function: auto log for Qt and console
function log(logtext) {

    if (isQtAvailable) {
        QtBridge.js_debug(logtext.toString());
    }
    else {
        console.log(logtext);
    }
}
