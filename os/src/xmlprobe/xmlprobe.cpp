// xmlprobe.cpp -- OURS: probe of the from-source XML stack matrixmenu's suite loader uses
// (xmlenginedom -> xmlengineutils -> xmlengine = libxml2 -> the P.I.P.S. libc).
// Parse a menu-suite-shaped document, walk it, read attributes, serialise it back.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <xml/dom/xmlengdom.h>
#include <xml/dom/xmlengdomparser.h>

_LIT8(KDoc, "<?xml version=\"1.0\"?><suite name=\"root\"><item id=\"1\" title=\"Messaging\"/>"
			"<item id=\"2\" title=\"Contacts\"/><folder><item id=\"3\" title=\"Nested &amp; escaped\"/></folder></suite>");

static void Walk(TXmlEngElement aEl, TInt aDepth)
	{
	TPtrC8 title = aEl.AttributeValueL(_L8("title"));
	TBuf8<64> t(aEl.Name().Left(20));
	TBuf8<64> v(title.Left(40));
	RDebug::Printf("[xmlprobe] %*c<%s> title='%s'", aDepth * 2 + 1, ' ', t.PtrZ(), v.PtrZ());
	RXmlEngNodeList<TXmlEngElement> kids;
	aEl.GetChildElements(kids);
	while (kids.HasNext())
		Walk(kids.Next(), aDepth + 1);
	kids.Close();
	}

static void RunL()
	{
	RDebug::Printf("[xmlprobe] RunL");
	RXmlEngDOMImplementation impl;
	impl.OpenL();
	RDebug::Printf("[xmlprobe] DOM implementation open");
	RXmlEngDOMParser parser;
	User::LeaveIfError(parser.Open(impl));
	RXmlEngDocument doc = parser.ParseL(KDoc);
	RDebug::Printf("[xmlprobe] parsed, last error %d", parser.GetLastParsingError());
	Walk(doc.DocumentElement(), 0);

	RBuf8 out;
	TXmlEngSerializationOptions opts;
	TInt n = doc.SaveL(out, doc, opts);
	RDebug::Printf("[xmlprobe] serialised %d bytes (input %d)", n, KDoc().Length());
	out.Close();
	doc.Close();
	parser.Close();
	impl.Close();
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[xmlprobe] start");
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, RunL());
	RDebug::Printf("[xmlprobe] done err=%d", err);
	delete cleanup;
	return err;
	}
