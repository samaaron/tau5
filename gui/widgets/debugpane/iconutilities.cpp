#include "iconutilities.h"
#include <QSvgRenderer>
#include <QPainter>
#include <QByteArray>

QIcon IconUtilities::createSvgIcon(const QString &normalSvg, const QString &hoverSvg, const QString &selectedSvg)
{
    QIcon icon;

    QByteArray normalBytes = normalSvg.toUtf8();
    QPixmap normalPixmap(32, 32);
    normalPixmap.fill(Qt::transparent);
    QSvgRenderer normalRenderer(normalBytes);
    if (normalRenderer.isValid())
    {
        QPainter painter(&normalPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        normalRenderer.render(&painter);
    }
    icon.addPixmap(normalPixmap, QIcon::Normal, QIcon::Off);

    if (!selectedSvg.isEmpty())
    {
        QByteArray selectedBytes = selectedSvg.toUtf8();
        QPixmap selectedPixmap(32, 32);
        selectedPixmap.fill(Qt::transparent);
        QSvgRenderer selectedRenderer(selectedBytes);
        if (selectedRenderer.isValid())
        {
            QPainter painter(&selectedPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            selectedRenderer.render(&painter);
        }
        icon.addPixmap(selectedPixmap, QIcon::Normal, QIcon::On);
    }
    
    return icon;
}

QPixmap IconUtilities::createSvgPixmap(const QString &svg, int width, int height)
{
    QByteArray svgBytes = svg.toUtf8();
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(svgBytes);
    if (renderer.isValid())
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
    }
    return pixmap;
}

QString IconUtilities::Icons::terminalSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 24 24' fill='%1'><path fill-rule='evenodd' clip-rule='evenodd' d='M1.5 3L3 1.5H21L22.5 3V21L21 22.5H3L1.5 21V3ZM3 3V21H21V3H3Z'/><path d='M7.06078 7.49988L6.00012 8.56054L10.2427 12.8032L6 17.0459L7.06066 18.1066L12 13.1673V12.4391L7.06078 7.49988Z'/><rect x='12' y='16.5' width='6' height='1.5'/></svg>").arg(color);
}

QString IconUtilities::Icons::bugSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1'><path fill-rule='evenodd' clip-rule='evenodd' d='M10.877 4.5v-.582a2.918 2.918 0 1 0-5.836 0V4.5h-.833L2.545 2.829l-.593.59 1.611 1.619-.019.049a8.03 8.03 0 0 0-.503 2.831c0 .196.007.39.02.58l.003.045H1v.836h2.169l.006.034c.172.941.504 1.802.954 2.531l.034.055L2.2 13.962l.592.592 1.871-1.872.058.066c.868.992 2.002 1.589 3.238 1.589 1.218 0 2.336-.579 3.199-1.544l.057-.064 1.91 1.92.593-.591-1.996-2.006.035-.056c.467-.74.81-1.619.986-2.583l.006-.034h2.171v-.836h-2.065l.003-.044a8.43 8.43 0 0 0 .02-.58 8.02 8.02 0 0 0-.517-2.866l-.019-.05 1.57-1.57-.592-.59L11.662 4.5h-.785zm-5 0v-.582a2.082 2.082 0 1 1 4.164 0V4.5H5.878z'/></svg>").arg(color);
}

QString IconUtilities::Icons::splitSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M14 1H3L2 2v11l1 1h11l1-1V2l-1-1zM8 13H3V2h5v11zm6 0H9V2h5v11z'/></svg>").arg(color);
}

QString IconUtilities::Icons::restartSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M12.75 8a4.5 4.5 0 0 1-8.61 1.834l-1.391.565A6.001 6.001 0 0 0 14.25 8 6 6 0 0 0 3.5 4.334V2.5H2v4l.75.75h3.5v-1.5H4.352A4.5 4.5 0 0 1 12.75 8z'/></svg>").arg(color);
}

QString IconUtilities::Icons::autoScrollOffSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'>"
        "<path d='M8 3v7M5 7l3 3 3-3' stroke='%1' stroke-width='1.5' fill='none'/>"
        "<rect x='4' y='12' width='8' height='2' fill='%1' opacity='0.3'/>"
        "</svg>").arg(color);
}

QString IconUtilities::Icons::autoScrollOnSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'>"
        "<path d='M8 3v7M5 7l3 3 3-3' stroke='%1' stroke-width='1.5' fill='none'/>"
        "<rect x='4' y='12' width='8' height='2' fill='%1'/>"
        "</svg>").arg(color);
}

QString IconUtilities::Icons::searchSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M15.7 13.3l-3.81-3.83A5.93 5.93 0 0 0 13 6c0-3.31-2.69-6-6-6S1 2.69 1 6s2.69 6 6 6c1.3 0 2.48-.41 3.47-1.11l3.83 3.81c.19.2.45.3.7.3.25 0 .52-.09.7-.3a.996.996 0 0 0 0-1.41v.01zM7 10.7c-2.59 0-4.7-2.11-4.7-4.7 0-2.59 2.11-4.7 4.7-4.7 2.59 0 4.7 2.11 4.7 4.7 0 2.59-2.11 4.7-4.7 4.7z'/></svg>").arg(color);
}

QString IconUtilities::Icons::zoomInSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M8 3v5H3v1h5v5h1V9h5V8H9V3H8z'/></svg>").arg(color);
}

QString IconUtilities::Icons::zoomOutSvg(const QString &color)
{
    return QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M3 8h10v1H3z'/></svg>").arg(color);
}