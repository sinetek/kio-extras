#ifndef __http_h__
#define __http_h__

#include <kio_interface.h>
#include <kio_base.h>

#include <string>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#include <qstack.h>
#include <qstring.h>

#include <k2url.h>

class HTTPProtocol : public IOProtocol
{
public:
  HTTPProtocol( Connection *_conn );
  virtual ~HTTPProtocol() { }

  enum HTTP_REV {HTTP_Unknown, HTTP_10, HTTP_11};
  enum HTTP_AUTH {AUTH_None, AUTH_Basic, AUTH_Digest, AUTH_Unknown};

  string realm;
  enum HTTP_REV HTTP;
  enum HTTP_AUTH Authentication;
  QStack<char> m_qEncodings;
  QByteArray big_buffer;

  virtual void slotGet( const char *_url );
  virtual void slotGetSize( const char *_url );
  virtual void slotCopy( const char *_source, const char *_dest );
  
  virtual bool error( int _err, const char *_txt );

  void jobError( int _errid, const char *_txt );
  
  Connection* connection() { return ConnectionSignals::m_pConnection; }

protected:

  void decodeChunked();
  void decodeGzip();
  size_t sendData();

  bool initSockaddr( struct sockaddr_in *server_name, const char *hostname, int port);
  bool http_open( K2URL &_url, const char* _post_data, int _post_data_len, bool _reload, unsigned long _offset = 0 );
  void http_close();

  void clearError() { m_iSavedError = 0; }
  void releaseError() {
    if ( m_iSavedError )
      IOProtocol::error( m_iSavedError, m_strSavedError.c_str() );
    m_iSavedError = 0;
  }

  string getUserAgentString();
  
  int m_cmd;

  FILE* m_fsocket;
  int m_sock;
  
  string m_strMimeType;
  int m_iSize;
  
  string m_strCharsets;
  string m_strLanguages;
  
  bool m_bUseProxy;
  string m_strNoProxyFor;
  string m_strProxyHost;
  string m_strProxyPort;
  string m_strProxyUser;
  string m_strProxyPass;
  struct sockaddr m_proxySockaddr;

  // Stuff to hold various error state information
  int m_iSavedError;
  string m_strSavedError;
  bool m_bIgnoreJobErrors, m_bIgnoreErrors; 

  bool m_bCanResume;
};

class HTTPIOJob : public IOJob
{
public:
  HTTPIOJob( Connection *_conn, HTTPProtocol *_gzip );
  
  virtual void slotError( int _errid, const char *_txt );

protected:
  HTTPProtocol* m_pHTTP;
};

#endif
