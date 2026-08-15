/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*!
  \class SoSFBitMask SoSFBitMask.h Inventor/fields/SoSFBitMask.h
  \brief The SoSFBitMask class is a container for a set of bit flags.

  \ingroup coin_fields

  This field is used where nodes, engines or other field containers
  needs to store one particular set of bit flags from an enumerated
  set.

  A field of this type stores its value to file as the symbolic
  names, rather than the actual integer bitmask. If more than one
  bit is set in the value when exporting, the bits are written like
  this: "(BITNAME|BITNAME|BITNAME|...)".

  SoSFBitMask instances are derived from SoSFEnum instances, and are
  initialized in the same way using the exact same enum-field-related
  macros.

  \sa SoMFBitMask, SoSFEnum
*/

// *************************************************************************

#include <Inventor/fields/SoSFBitMask.h>

#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>

#include "fields/SoSubFieldP.h"

// *************************************************************************

SO_SFIELD_DERIVED_SOURCE(SoSFBitMask, not_used, not_used);

// *************************************************************************

/*!
  \copydetails SoField::initClass(void)
*/
void
SoSFBitMask::initClass(void)
{
  SO_SFIELD_INTERNAL_INIT_CLASS(SoSFBitMask);
}


// No need to document readValue() and writeValue() here, as the
// necessary information is provided by the documentation of the
// parent classes.
#ifndef DOXYGEN_SKIP_THIS

SbBool
SoSFBitMask::readValue(SoInput * in)
{
  // FIXME: in this case, perhaps we should rather accept numeric
  // values instead of demanding mnemonics? 20020630 mortene.
  if (!this->legalValuesSet) {
    SbName name;
    SoFieldContainer * thecontainer = this->getContainer();
    SbBool fname = thecontainer && thecontainer->getFieldName(this, name);
    SoReadError::post(in,
                      "no mappings available for SoSFBitMask field %s",
                      fname ? name.getString() : "");
    return FALSE;
  }

  if (in->isBinary()) {
    int bitmask = 0;
    while (TRUE) {
      SbName n;
      if (!in->read(n, TRUE)) {
        SoReadError::post(in, "Couldn't read SoSFBitMask bitmask value");
        return FALSE;
      }
      if (n.getLength() == 0) break;

      int v;
      if (!this->findEnumValue(n, v)) {
        SoReadError::post(in, "Unknown SoSFBitMask bit "
                          "mask value \"%s\"", n.getString());
        return FALSE;
      }

      bitmask |= v;
    }

    this->value = bitmask;
    return TRUE;
  }


  // Read first character
  char c;
  if (!in->read(c)) {
    SoReadError::post(in, "Premature end of file");
    return FALSE;
  }

  // Check for parenthesized list of bitwise-or'ed flags
  if (c == '(') {
    int bitmask = 0;

    // Read names separated by '|'
    while (TRUE) {
      SbName n;
      if (in->read(n, TRUE) && !(!n)) {
        int v;
        if (!this->findEnumValue(n, v)) {
          SoReadError::post(in, "Unknown SoSFBitMask bit "
                            "mask value \"%s\"", n.getString());
          return FALSE;
        }

        bitmask |= v;
      }

      if (!in->read(c)) {
        SoReadError::post(in, "EOF reached before ')' in SoSFBitMask value");
        return FALSE;
      }

      if (c == ')') break;
      else if (c != '|') {
        SoReadError::post(in,
                          "Expected '|' or ')', got '%c' in SoSFBitMask value",
                          c);
        return FALSE;
      }
    }
    this->value = bitmask;
  }
  else {
    in->putBack(c);

    // Read mnemonic value as a character string identifier
    SbName n;
    if (!in->read(n, TRUE)) {
      SoReadError::post(in, "Couldn't read SoSFBitMask bit name");
      return FALSE;
    }

    int v;
    if (!this->findEnumValue(n, v)) {
      SoReadError::post(in, "Unknown SoSFBitMask bit mask value \"%s\"",
                        n.getString());
      return FALSE;
    }

    this->value = v;
  }

  return TRUE;
}

void
SoSFBitMask::writeValue(SoOutput * out) const
{
  SbBool paran = FALSE;
  int out_vals_written = 0;

  // FIXME: as enumValues for SoSFBitMasks can be OR'ed combinations
  // of other enumValues, we should try to write a minimal set of
  // flags by masking out flags with the highest number of bits
  // first. 19991110 mortene.

  int restval = this->getValue();
  int i = 0;
  while (restval) {
    if (i >= this->numEnums) break;
    if (this->enumValues[i] & restval) {
      restval &= ~this->enumValues[i];
      if (!out_vals_written && restval) {
        if (!out->isBinary()) out->write('(');
        paran = TRUE;
      }
      if (out_vals_written++ && !out->isBinary()) out->write(" | ");
      out->write(static_cast<const char *>(this->enumNames[i].getString()));
    }

    i++;
  }
  if (paran && !out->isBinary()) out->write(')');
  if (!out_vals_written && !out->isBinary()) out->write("()");

  if (out->isBinary()) out->write(0x00000000);

#if COIN_DEBUG
  if (restval) {
    SoDebugError::post("SoSFBitMask::writeValue",
                       "invalid bitmask -- some bits \"lost\" (0x%x) "
                       "upon export",
                       restval);
  }
#endif // COIN_DEBUG
}

#endif // DOXYGEN_SKIP_THIS
