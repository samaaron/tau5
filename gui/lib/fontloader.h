#ifndef FONTLOADER_H
#define FONTLOADER_H

#include <QString>
#include <QByteArray>

class FontLoader
{
public:
    // Load font file and convert to base64 data URI
    static QString loadFontAsDataUri(const QString &resourcePath);
    
    // Generate @font-face CSS with embedded base64 font
    static QString generateFontFaceCss(const QString &fontFamily, 
                                      const QString &resourcePath,
                                      const QString &format = "truetype");
    
    // Get complete CSS for Cascadia Code font injection
    static QString getCascadiaCodeCss();
    
private:
    static QByteArray loadResourceFile(const QString &resourcePath);
    static QString toBase64DataUri(const QByteArray &data, const QString &mimeType);
};

#endif // FONTLOADER_H