/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package tests.api.org.xml.sax.support;

import org.xml.sax.DTDHandler;
import org.xml.sax.DocumentHandler;
import org.xml.sax.EntityResolver;
import org.xml.sax.ErrorHandler;
import org.xml.sax.InputSource;
import org.xml.sax.Parser;

import java.util.Locale;

/**
 * A SAX Parser that can be accessed, but not instantiated.
 */
@SuppressWarnings("deprecation")
public class NoInstanceParser implements Parser {

    public NoInstanceParser(int i) {
    }

    public void parse(InputSource source)  {
    }

    public void parse(String systemId) {
    }

    public void setDocumentHandler(DocumentHandler handler) {
    }

    public void setDTDHandler(DTDHandler handler) {
    }

    public void setEntityResolver(EntityResolver resolver) {
    }

    public void setErrorHandler(ErrorHandler handler) {
    }

    public void setLocale(Locale locale) {
    }

}
