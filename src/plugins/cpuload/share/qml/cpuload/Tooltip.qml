import QtQuick 1.1
import org.LC.common 1.0

Rectangle {
    id: rootRect
    width: loadView.cellWidth * 2
    height: loadView.cellHeight * loadView.rows

    smooth: true
    radius: 5

    signal closeRequested()

    function beforeDelete() {}

    property alias isHovered: wholeArea.containsMouse

    MouseArea {
        id: wholeArea
        hoverEnabled: true
    }

    function zipN(first) {
        for (var i = 1; i < arguments.length; ++i) {
            var other = arguments[i];
            first = cpuProxy.sumPoints(first, other);
        }
        return first;
    }

    Grid {
        id: loadView

        anchors.fill: parent

        rows: Math.ceil(loadRepeater.count / 2)
        columns: 2

        property int cellWidth: 400
        property int cellHeight: 120

        Repeater {
            id: loadRepeater
            model: loadModel

            Rectangle {
                width: 400
                height: plot.height

                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: colorProxy.color_TextBox_TopColor
                    }
                    GradientStop {
                        position: 1
                        color: colorProxy.color_TextBox_BottomColor
                    }
                }

                Plot {
                    id: plot

                    anchors.top: parent.top

                    width: parent.width
                    height: 120

                    multipoints: [
                            { color: "red", points: zipN(loadObj.ioHist, loadObj.lowHist, loadObj.mediumHist, loadObj.highHist) },
                            { color: "blue", points: zipN(loadObj.ioHist, loadObj.lowHist, loadObj.mediumHist) },
                            { color: "yellow", points: zipN(loadObj.ioHist, loadObj.lowHist) },
                            { color: "green", points: loadObj.ioHist }
                        ]

                    leftAxisEnabled: true
                    leftAxisTitle: qsTr("Load, %")
                    yGridEnabled: true

                    plotTitle: "CPU " + cpuIdx

                    minYValue: 0
                    maxYValue: 100
                    minXValue: 0
                    maxXValue: loadObj.getPointsCount() - 1

                    alpha: 1
                    background: "transparent"
                    textColor: colorProxy.color_TextBox_TextColor
                    gridLinesColor: colorProxy.color_TextBox_Aux2TextColor
                }
            }
        }
    }
}
