#ifndef SHADERPAGE_H
#define SHADERPAGE_H

#include <QString>

class ShaderPage {
public:
    // Get the complete HTML page with embedded shader
    // This is a singleton-like static method that generates the page once
    static const QString& getHtml();
    
private:
    static QString generateHtml();
    static QString s_cachedHtml;
    static bool s_initialized;
};

#endif // SHADERPAGE_H