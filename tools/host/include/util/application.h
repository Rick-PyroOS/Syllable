/* Host shim: there is no Application on the host.  util/catalog.cpp asks it
 * for the application catalog in LString(); GetCatalog() returns NULL, so
 * LString throws GeneralFailure("Catalog not loaded!"), as on Syllable. */
#ifndef __F_UTIL_APPLICATION_H__
#define __F_UTIL_APPLICATION_H__

namespace os {

class Catalog;

class Application
{
public:
    static Application* GetInstance();
    const Catalog* GetCatalog() const;
};

} // end of namespace

#endif /* __F_UTIL_APPLICATION_H__ */
