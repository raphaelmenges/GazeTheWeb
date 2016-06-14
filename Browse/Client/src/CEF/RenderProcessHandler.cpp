//============================================================================
// Distributed under the Apache License, Version 2.0.
// Author: Daniel M�ller (muellerd@uni-koblenz.de)
//============================================================================

#include "RenderProcessHandler.h"
#include "src/CEF/Extension/Container.h"
#include "include/base/cef_logging.h"
#include "include/wrapper/cef_helpers.h"
#include <sstream>

bool RenderProcessHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefProcessId sourceProcess,
    CefRefPtr<CefProcessMessage> msg)
{
    CEF_REQUIRE_RENDERER_THREAD();

    const std::string& msgName = msg->GetName().ToString();
    IPCLogDebug(browser, "Received '" + msgName + "' IPC msg in RenderProcessHandler");

    // Handle request of DOM node data
    if (msgName == "GetDOMElements")
    {
        long long frameID = (long long) msg->GetArgumentList()->GetDouble(0);
        CefRefPtr<CefFrame> frame = browser->GetFrame(frameID);

        CefRefPtr<CefV8Context> context = frame->GetV8Context();
        // Return global object for this context
        CefRefPtr<CefV8Value> global = context->GetGlobal();

        // Determine number of each DOM element
        IPCLogDebug(browser, "Updating size of text input array via Javascript");
        frame->ExecuteJavaScript(_js_dom_update_sizes, frame->GetURL(), 0);

        int arraySize = 0;
        if (context->Enter())
        {
            arraySize += global->GetValue("sizeTextInputs")->GetUIntValue();
            context->Exit();
        }
        IPCLogDebug(browser, "Found " + std::to_string(arraySize) + " objects to be packed in IPC");

        if (arraySize > 0)
        {
            // Add array of container arrays to JS
            V8_Container v8Container(domNodeScheme);
            v8Container.AddContainerArray(context, "TextInputs", arraySize);

            // Fill container array in JS with data
            frame->ExecuteJavaScript(_js_dom_fill_arrays, frame->GetURL(), 0);

            // Create IPC message, which is to be filled with data
            CefRefPtr<CefProcessMessage> DOMmsg = CefProcessMessage::Create("ReceiveDOMElements");

            // Read out data in JS container array and write it to IPC message
            IPC_Container ipcContainer(domNodeScheme);
            ipcContainer.ReadContainerObjectsAndWriteToIPCMsg(
                context,										// Frame's V8 context in order to read out JS variables
                "TextInputs",									// Container arrays variable name in JS
                std::vector<int>{ arraySize },					// Amount of each different node types
                frameID,										// First position in message is frameID
				DOMmsg);											// Message to be filled with value's from JS container array

            // Send read-out DOM node data to browser process
            browser->SendProcessMessage(PID_BROWSER, DOMmsg);
            IPCLogDebug(browser, "Finished reading DOM node data, sending IPC msg to Handler with node information");
        }
        return true;

    }

    // EXPERIMENTAL: Handle request of favicon image bytes
    if (msgName == "GetFavIconBytes")
    {
        CefRefPtr<CefFrame> frame = browser->GetMainFrame();
        CefRefPtr<CefV8Context> context = frame->GetV8Context();

        std::string lastFaviconURL = msg->GetArgumentList()->GetString(0);

        // Create process message, which is to be sent to Handler
        msg = CefProcessMessage::Create("ReceiveFavIconBytes");
        CefRefPtr<CefListValue> args = msg->GetArgumentList();


        std::string favIconURL = "";
        int height = -2, width = -2;

        if (context->Enter())
        {
            CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

            // Read out values in width and height
            favIconURL = globalObj->GetValue("favIconUrl")->GetStringValue();
            height = globalObj->GetValue("favIconHeight")->GetIntValue();
            width = globalObj->GetValue("favIconWidth")->GetIntValue();

            // Fill msg args with this index
            int index = 0;
            args->SetString(index++, favIconURL);
            // Write image resolution at the beginning after URL
            args->SetInt(index++, width);
            args->SetInt(index++, height);

            // Skip reading favicon image's byte data, when URL hasn't changed
            if (favIconURL != lastFaviconURL || width <= 0 || height <= 0)
            {
                IPCLogDebug(browser, "RenderProcessHandler: Load new favicon URL: '" + favIconURL + "' (w: " + std::to_string(width) +", h: " + std::to_string(height) + ")");
                //Create byte array in JS for image data
                CefRefPtr<CefV8Value> byteArray = CefV8Value::CreateArray(width*height); // Combine 4 * byte in one int value via bit shifting
                for (int i = 0; i < width*height; i++)
                {
                    byteArray->SetValue(i, CefV8Value::CreateInt(0));
                }
                globalObj->SetValue("favIconData", byteArray, V8_PROPERTY_ATTRIBUTE_NONE);

                // Fill byte array with JS
                browser->GetMainFrame()->ExecuteJavaScript(_js_favicon_copy_img_bytes_to_v8array, browser->GetMainFrame()->GetURL(), 0);

                // Read out each byte and write it to IPC msg
                byteArray = context->GetGlobal()->GetValue("favIconData");
                for (int i = 0; i < width*height; i++)
                {
                    args->SetInt(index++, byteArray->GetValue(i)->GetIntValue());
                    byteArray->DeleteValue(i);
                }

                // Release V8 value afterwards
                globalObj->DeleteValue("favIconData");
            }
            else
            {
                if(favIconURL != "")
                    IPCLogDebug(browser, "RenderProcessHandler: old and new favicon URL are identical: '" + favIconURL +"'");
                else
                    IPCLogDebug(browser, "RenderProcessHandler: ERROR: Read empty favIconURL!");
            }
            context->Exit();
        }
        browser->SendProcessMessage(PID_BROWSER, msg);
    }

    if (msgName == "GetPageResolution")
    {
        CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();

        if (context->Enter())
        {
            CefRefPtr<CefV8Value> global = context->GetGlobal();

            double pageWidth = global->GetValue("_pageWidth")->GetDoubleValue();
            double pageHeight = global->GetValue("_pageHeight")->GetDoubleValue();

            msg = CefProcessMessage::Create("ReceivePageResolution");
            msg->GetArgumentList()->SetDouble(0, pageWidth);
            msg->GetArgumentList()->SetDouble(1, pageHeight);
            browser->SendProcessMessage(PID_BROWSER, msg);

            context->Exit();
        }
        return true;
    }

    // EXPERIMENTAL: Handle request of fixed elements' coordinates
    if (msgName == "GetFixedElements")
    {
        //CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();

        //if (context->Enter())
        //{
        //	CefRefPtr<CefV8Value> global = context->GetGlobal();

        //	int number_of_fixed_elements = 5;
        //	CefRefPtr<CefV8Value> fixedElemArray = CefV8Value::CreateArray(4 * number_of_fixed_elements);

        //	for (int i = 0; i < 4 * number_of_fixed_elements; i++)
        //	{
        //		fixedElemArray->SetValue(i, CefV8Value::CreateDouble(-1));
        //	}

        //	global->SetValue("_fixedElements", fixedElemArray , V8_PROPERTY_ATTRIBUTE_NONE);

            // Execute JS to fill V8 array with coordinates
            // browser->GetMainFrame()->ExecuteJavaScript(_js_fixed_element_read_out, browser->GetMainFrame()->GetURL(), 0);

        //	msg = CefProcessMessage::Create("ReceiveFixedElements");
        //	CefRefPtr<CefListValue> args = msg->GetArgumentList();


        //	for (int i = 0; i < 4 * number_of_fixed_elements; i++)
        //	{
        //		double coordinate = fixedElemArray->GetValue(i)->GetDoubleValue();

        //		if (coordinate == -1)
        //			break;

        //		args->SetDouble(i, coordinate);
        //	}

            // Send process msg
            // browser->SendProcessMessage(PID_BROWSER, msg);

        //	// TODO: Send process msg

        //	// TODO: Release V8 values


            // Release V8 values in JS
            // global->DeleteValue("_fixedElements");

        //	context->Exit();
        //}
    }

    // If no suitable handling was found
    return false;
}

void RenderProcessHandler::OnFocusedNodeChanged(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefDOMNode> node)
{
    // TODO, if needed
}

void RenderProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context)
{
    if (frame->IsMain())
    {
    // Create variables in Javascript which are to be read after page finished loading.
    // Variables here contain the amount of needed objects in order to allocate arrays, which are just big enough
        if (context->Enter())
        {

            // Retrieve the context's window object.
            CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

            // Add attributes with their pre-set values to JS object |window|
            globalObj->SetValue("_pageWidth", CefV8Value::CreateDouble(-1), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("_pageHeight", CefV8Value::CreateDouble(-1), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("sizeTextLinks", CefV8Value::CreateInt(0), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("sizeTextInputs", CefV8Value::CreateInt(0), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("_pageHeight", CefV8Value::CreateDouble(-1), V8_PROPERTY_ATTRIBUTE_NONE);

            // Create JS variables for width and height of icon image
            globalObj->SetValue("favIconUrl", CefV8Value::CreateString(""), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("favIconHeight", CefV8Value::CreateInt(-1), V8_PROPERTY_ATTRIBUTE_NONE);
            globalObj->SetValue("favIconWidth", CefV8Value::CreateInt(-1), V8_PROPERTY_ATTRIBUTE_NONE);

            frame->ExecuteJavaScript(_js_favicon_create_img, frame->GetURL(), 0);
            //// Fill URL, width and height variables with values (via Javascript)
            //frame->ExecuteJavaScript(JS_SET_FAVICON_URL_RESOLUTION, frame->GetURL(), 0);

            context->Exit();
        }
        /*
        *	GetFavIconBytes
        * END *******************************************************************************/

    }
    else IPCLogDebug(browser, "Not able to enter context!");
}

void RenderProcessHandler::OnContextReleased(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context)
{
    if (frame->IsMain())
    {
        // Release all created V8 values, when context is released
        CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

        globalObj->DeleteValue("_pageWidth");
        globalObj->DeleteValue("_pageHeight");

        globalObj->DeleteValue("sizeTextLinks");
        globalObj->DeleteValue("sizeTextInputs");

        globalObj->DeleteValue("favIconUrl");
        globalObj->DeleteValue("favIconHeight");
        globalObj->DeleteValue("favIconWidth");
    }
}

void RenderProcessHandler::IPCLogDebug(CefRefPtr<CefBrowser> browser, std::string text)
{
    IPCLog(browser, text, true);
}

void RenderProcessHandler::IPCLog(CefRefPtr<CefBrowser> browser, std::string text, bool debugLog)
{
    CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("IPCLog");
    msg->GetArgumentList()->SetBool(0, debugLog);
    msg->GetArgumentList()->SetString(1, text);
    browser->SendProcessMessage(PID_BROWSER, msg);

    // Just in case (time offset in log file due to slow IPC msg, for example): Use CEF's logging as well
    if (debugLog)
    {
        DLOG(INFO) << text;
    }
    else
    {
        LOG(INFO) << text;
    }
}