#ifndef ICONUTILITIES_H
#define ICONUTILITIES_H

#include <QIcon>
#include <QPixmap>
#include <QString>

class IconUtilities
{
public:
    static QIcon createSvgIcon(const QString &normalSvg, const QString &hoverSvg = "", const QString &selectedSvg = "");
    static QPixmap createSvgPixmap(const QString &svg, int width, int height);
    
    // SVG icon definitions
    struct Icons {
        static QString terminalSvg(const QString &color);
        static QString bugSvg(const QString &color);
        static QString splitSvg(const QString &color);
        static QString restartSvg(const QString &color);
        static QString autoScrollOffSvg(const QString &color);
        static QString autoScrollOnSvg(const QString &color);
        static QString searchSvg(const QString &color);
        static QString zoomInSvg(const QString &color);
        static QString zoomOutSvg(const QString &color);
    };
};

#endif // ICONUTILITIES_H