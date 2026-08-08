#include "macscreencapture.hh"

#import <AppKit/AppKit.h>
#import <Vision/Vision.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <QString>
#include <QPoint>

const int     captureIntervalMs = 300;
const int     pollIntervalMs    = 100;
const CGFloat captureRegionW    = 400;
const CGFloat captureRegionH    = 100;

static dispatch_queue_t captureQueue()
{
  static dispatch_queue_t q;
  static dispatch_once_t   once;
  dispatch_once( &once, ^{
    q = dispatch_queue_create( "goldendict.capture", DISPATCH_QUEUE_SERIAL );
  } );
  return q;
}

// Tokenize a string into word/subword ranges suitable for per-word bounding boxes.
// - Latin/Cyrillic: splits on whitespace & punctuation boundaries
// - CJK ideographs: each character is its own token
static NSArray<NSValue *> * tokenRangesForText( NSString * text )
{
  NSMutableArray<NSValue *> * ranges = [NSMutableArray array];
  if ( !text.length ) return ranges;

  NSUInteger len  = text.length;
  NSUInteger start = 0;
  BOOL inLatinWord = NO;

  for ( NSUInteger i = 0; i < len; ) {
    unichar ch = [text characterAtIndex:i];
    BOOL isCJK = ( ch >= 0x4E00 && ch <= 0x9FFF )   // CJK Unified
              || ( ch >= 0x3400 && ch <= 0x4DBF )    // CJK Ext-A
              || ( ch >= 0xF900 && ch <= 0xFAFF )    // CJK Compat
              || ( ch >= 0x3040 && ch <= 0x30FF )    // Hiragana/Katakana
              || ( ch >= 0xAC00 && ch <= 0xD7AF );   // Hangul

    if ( isCJK ) {
      if ( inLatinWord && i > start ) {
        [ranges addObject:[NSValue valueWithRange:NSMakeRange( start, i - start )]];
      }
      [ranges addObject:[NSValue valueWithRange:NSMakeRange( i, 1 )]];
      start = i + 1;
      inLatinWord = NO;
      ++i;
    }
    else {
      unichar nextSep = ch;
      BOOL isSep = ( nextSep == ' ' || nextSep == '\t' || nextSep == '\n'
                  || nextSep == '\r' || nextSep == ',' || nextSep == '.'
                  || nextSep == ';' || nextSep == ':' || nextSep == '!'
                  || nextSep == '?' || nextSep == '"' || nextSep == '\''
                  || nextSep == '(' || nextSep == ')' || nextSep == '['
                  || nextSep == ']' || nextSep == '{' || nextSep == '}'
                  || nextSep == '/' || nextSep == '\\' || nextSep == '-'
                  || nextSep == '_' || nextSep == '@' || nextSep == '#'
                  || nextSep == '$' || nextSep == '%' || nextSep == '&'
                  || nextSep == '*' || nextSep == '+' || nextSep == '='
                  || nextSep == '<' || nextSep == '>' );
      if ( isSep ) {
        if ( inLatinWord && i > start ) {
          [ranges addObject:[NSValue valueWithRange:NSMakeRange( start, i - start )]];
        }
        start = i + 1;
        inLatinWord = NO;
      }
      else {
        if ( !inLatinWord ) {
          start = i;
          inLatinWord = YES;
        }
      }
      ++i;
    }
  }

  if ( inLatinWord && len > start ) {
    [ranges addObject:[NSValue valueWithRange:NSMakeRange( start, len - start )]];
  }

  return ranges;
}

// ---------------------------------------------------------------------------
// CGEventTap callback
// ---------------------------------------------------------------------------
static CGEventRef captureEventCallback( CGEventTapProxy proxy,
                                        CGEventType type,
                                        CGEventRef event,
                                        void * refcon )
{
  (void)proxy;
  if ( type == kCGEventMouseMoved )
    static_cast<MacScreenCapture *>( refcon )->mouseMoved();
  return event;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
MacScreenCapture & MacScreenCapture::instance()
{
  static MacScreenCapture m;
  return m;
}

MacScreenCapture::MacScreenCapture()
  : pPref( nullptr ), tapRef( nullptr ), loop( nullptr ), usePolling( false )
{
  captureTimer.setSingleShot( true );
  connect( &captureTimer, SIGNAL( timeout() ), this, SLOT( timerShot() ) );

  pollTimer.setInterval( pollIntervalMs );
  connect( &pollTimer, SIGNAL( timeout() ), this, SLOT( pollMousePosition() ) );
}

MacScreenCapture::~MacScreenCapture()
{
  disableCapture();
  if ( tapRef ) CFRelease( tapRef );
  if ( loop )   CFRelease( loop );
}

bool MacScreenCapture::isAvailable()
{
  return CGPreflightScreenCaptureAccess();
}

// ---------------------------------------------------------------------------
// Enable – pure polling, no Accessibility permission needed
// ---------------------------------------------------------------------------
void MacScreenCapture::enableCapture()
{
  captureTimer.stop();
  pollTimer.stop();
  usePolling = true;
  lastPollPos = QPoint( -1, -1 );
  pollTimer.start();
}

void MacScreenCapture::disableCapture()
{
  captureTimer.stop();
  pollTimer.stop();
  if ( loop )
    CFRunLoopRemoveSource( CFRunLoopGetMain(), loop, kCFRunLoopCommonModes );
}

// ---------------------------------------------------------------------------
// Mouse moved (from CGEventTap or poll) → restart debounce timer
// ---------------------------------------------------------------------------
void MacScreenCapture::mouseMoved()
{
  captureTimer.start( captureIntervalMs );
}

void MacScreenCapture::pollMousePosition()
{
  NSPoint loc = [NSEvent mouseLocation];
  QPoint  pos( (int)loc.x, (int)loc.y );
  if ( pos != lastPollPos ) {
    lastPollPos = pos;
    mouseMoved();
  }
}

// ---------------------------------------------------------------------------
// Debounce timer fired → dispatch capture + OCR to background
// ---------------------------------------------------------------------------
void MacScreenCapture::timerShot()
{
  if ( !captureMutex.tryLock( 0 ) ) return;
  captureMutex.unlock();

  if ( !pPref ) return;

  if ( pPref->enableScanPopupModifiers
       && !checkModifiersPressed( pPref->scanPopupModifiers ) )
    return;

  // Gather coordinates on main thread
  NSPoint ml = [NSEvent mouseLocation];
  NSScreen * screen = nil;
  for ( NSScreen * s in [NSScreen screens] ) {
    if ( NSPointInRect( ml, [s frame] ) ) { screen = s; break; }
  }
  if ( !screen ) return;

  NSRect   sf    = [screen frame];
  CGFloat  scale = [screen backingScaleFactor];
  NSNumber * dn  = [screen deviceDescription][@"NSScreenNumber"];
  if ( !dn ) return;
  CGDirectDisplayID displayID = (CGDirectDisplayID)[dn unsignedIntValue];

  CGFloat ox = ml.x - captureRegionW / 2.0;
  CGFloat oy = ml.y - captureRegionH / 2.0;
  if ( ox < sf.origin.x ) ox = sf.origin.x;
  if ( oy < sf.origin.y ) oy = sf.origin.y;
  if ( ox + captureRegionW > sf.origin.x + sf.size.width )
    ox = sf.origin.x + sf.size.width - captureRegionW;
  if ( oy + captureRegionH > sf.origin.y + sf.size.height )
    oy = sf.origin.y + sf.size.height - captureRegionH;

  CGRect capRect     = CGRectMake( ox, oy, captureRegionW, captureRegionH );
  CGFloat mxNorm     = ( ml.x - capRect.origin.x ) / captureRegionW;
  CGFloat myNorm     = ( ml.y - capRect.origin.y ) / captureRegionH;
  CGPoint mouseNorm  = CGPointMake( mxNorm, myNorm );

  CGFloat lx  = capRect.origin.x - sf.origin.x;
  CGFloat ly  = capRect.origin.y - sf.origin.y;
  CGFloat fy  = sf.size.height - ( ly + capRect.size.height );
  CGRect  qr  = CGRectMake( lx, fy, capRect.size.width, capRect.size.height );
  size_t  pw  = (size_t)( capRect.size.width * scale );
  size_t  ph  = (size_t)( capRect.size.height * scale );
  if ( pw < 8 ) pw = 8;  if ( ph < 8 ) ph = 8;

  dispatch_async( captureQueue(), ^{
    [SCShareableContent getShareableContentWithCompletionHandler:
        ^( SCShareableContent * content, NSError * err ) {
      if ( err || !content ) return;

      SCDisplay * scD = nil;
      for ( SCDisplay * d in content.displays )
        if ( d.displayID == displayID ) { scD = d; break; }
      if ( !scD ) return;

      SCContentFilter * filter = [[SCContentFilter alloc]
          initWithDisplay:scD excludingWindows:@[]];
      SCStreamConfiguration * cfg = [[SCStreamConfiguration alloc] init];
      cfg.sourceRect  = qr;
      cfg.width       = (int)pw;
      cfg.height      = (int)ph;
      cfg.queueDepth  = 1;
      cfg.pixelFormat = kCVPixelFormatType_32BGRA;
      cfg.showsCursor = NO;

      [SCScreenshotManager captureImageWithFilter:filter
                                    configuration:cfg
                                completionHandler:
          ^( CGImageRef img, NSError * capErr ) {
        if ( !img || capErr ) return;

        VNRecognizeTextRequest * req = [[VNRecognizeTextRequest alloc]
            initWithCompletionHandler:^( VNRequest * r, NSError * ocrErr ) {
          if ( ocrErr || !r.results || !r.results.count ) return;

          // Build list of (word, boundingBox) from all observations,
          // tokenizing each observation so we pick the single word under cursor.
          NSMutableArray<NSValue *> * wordBoxes = [NSMutableArray array];
          NSMutableArray<NSString *> * wordTexts = [NSMutableArray array];

          for ( VNRecognizedTextObservation * obs in r.results ) {
            VNRecognizedText * top = [obs topCandidates:1].firstObject;
            if ( !top || !top.string.length ) continue;

            NSString * fullText = top.string;
            NSArray<NSValue *> * tokenRanges = tokenRangesForText( fullText );
            for ( NSValue * rangeVal in tokenRanges ) {
              NSRange tokenRange = [rangeVal rangeValue];
              NSString * token = [fullText substringWithRange:tokenRange];
              // Skip pure-whitespace / punctuation-only tokens
              NSString * trimmed = [token stringByTrimmingCharactersInSet:
                  [NSCharacterSet whitespaceAndNewlineCharacterSet]];
              if ( !trimmed.length ) continue;

              // Estimate token position by character ratio within observation
              CGFloat startFrac = (CGFloat)tokenRange.location / (CGFloat)fullText.length;
              CGFloat widthFrac = (CGFloat)tokenRange.length / (CGFloat)fullText.length;
              CGRect tokenBox = CGRectMake(
                obs.boundingBox.origin.x + obs.boundingBox.size.width * startFrac,
                obs.boundingBox.origin.y,
                obs.boundingBox.size.width * widthFrac,
                obs.boundingBox.size.height );

              [wordBoxes addObject:[NSValue valueWithRect:tokenBox]];
              [wordTexts addObject:trimmed];
            }
          }

          if ( !wordTexts.count ) return;

          // Find the token whose bounding box contains the cursor (tightest match),
          // or nearest by centre distance if no box contains cursor.
          CGFloat bestD = CGFLOAT_MAX;
          NSString * bestS = nil;

          for ( NSUInteger i = 0; i < wordBoxes.count; ++i ) {
            CGRect box = [wordBoxes[i] rectValue];
            if ( CGRectContainsPoint( box, mouseNorm ) ) {
              // Cursor inside this token's box — pick it immediately
              bestS = wordTexts[i];
              break;
            }
            CGFloat dx = CGRectGetMidX( box ) - mouseNorm.x;
            CGFloat dy = CGRectGetMidY( box ) - mouseNorm.y;
            CGFloat d  = dx * dx + dy * dy;
            if ( d < bestD ) { bestD = d; bestS = wordTexts[i]; }
          }

          if ( !bestS || !bestS.length ) return;

          QString word = QString::fromNSString( bestS ).trimmed();
          if ( word.isEmpty() ) return;

          for ( int i = 0; i < word.size(); ++i ) {
            QChar::Direction dir = word[ i ].direction();
            if ( dir == QChar::DirR || dir == QChar::DirAL
                 || dir == QChar::DirRLE || dir == QChar::DirRLO ) {
              std::reverse( word.begin(), word.end() );
              break;
            }
          }

          dispatch_async( dispatch_get_main_queue(), ^{
            emit MacScreenCapture::instance().hovered( word, false );
          } );
        }];

        req.recognitionLevel       = VNRequestTextRecognitionLevelAccurate;
        req.recognitionLanguages   = @[ @"zh-Hans", @"zh-Hant", @"en", @"ja", @"ko" ];
        req.usesLanguageCorrection = NO;
        req.minimumTextHeight      = 0.01;

        VNImageRequestHandler * vh =
            [[VNImageRequestHandler alloc] initWithCGImage:img options:@{}];
        [vh performRequests:@[ req ] error:nil];
      }];
    }];
  } );
}
