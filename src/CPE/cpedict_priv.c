/**
 * @file cpedict_priv.c
 * \brief Interface to Common Platform Enumeration (CPE) Language
 *
 * See more details at http://nvd.nist.gov/cpe.cfm
 */

/*
 * Copyright 2008 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Maros Barabas <mbarabas@redhat.com>
 */


#include <libxml/xmlreader.h>
#include <stdlib.h>

#include "cpedict_priv.h"
#include "cpeuri.h"

// for functions isspace
#include <ctype.h>
// for functins memset, strcpy
#include <string.h>

#include "../common/list.h"

/***************************************************************************/
/* Variable definitions
 * */

const char *PART_TO_CHAR[] = { NULL, "h", "o", "a" };

/* ****************************************
 * CPE-List structures
 * ***************************************/

/* <cpe-item>
 * */
struct cpe_item {                   // the node <cpe-item>

        struct xml_metadata xml;
	struct cpe_name *name;		// CPE name as CPE URI
        struct oscap_list* titles;      // titles of cpe-item (can be in various languages)

	struct cpe_name *deprecated;   // CPE that deprecated this one (or NULL)
	char *deprecation_date;	       // date of deprecation

	struct oscap_list* references; // list of references
	struct oscap_list* checks;     // list of checks
	struct oscap_list* notes;      // list of notes - it's the same structure as titles
        struct cpe_item_metadata *metadata;    // element <meta:item-metadata>
};
OSCAP_GETTER(struct cpe_name*, cpe_item, name)
OSCAP_GETTER(struct cpe_name*, cpe_item, deprecated)
OSCAP_ACCESSOR_STRING(cpe_item, deprecation_date)
OSCAP_GETTER(struct cpe_item_metadata*, cpe_item, metadata)
OSCAP_IGETINS_GEN(cpe_reference, cpe_item, references, reference)
OSCAP_IGETINS_GEN(cpe_check, cpe_item, checks, check)
OSCAP_IGETINS(oscap_title, cpe_item, titles, title)
OSCAP_IGETINS(oscap_title, cpe_item, notes, note)

/* <cpe-item><item-metadata>
 * */
struct cpe_item_metadata {
        struct xml_metadata xml;
        char *modification_date;
        char *status;
        char *nvd_id;
        char *deprecated_by_nvd_id;
};
OSCAP_ACCESSOR_STRING(cpe_item_metadata, modification_date)
OSCAP_ACCESSOR_STRING(cpe_item_metadata, status)
OSCAP_ACCESSOR_STRING(cpe_item_metadata, nvd_id)
OSCAP_ACCESSOR_STRING(cpe_item_metadata, deprecated_by_nvd_id)

/* <cpe-item><check>
 * */
struct cpe_check {
        struct xml_metadata xml;
	char *system;      // system check URI
	char *href;        // external file reference (NULL if not present)
	char *identifier;  // test identifier
};
OSCAP_ACCESSOR_STRING(cpe_check, system)
OSCAP_ACCESSOR_STRING(cpe_check, href)
OSCAP_ACCESSOR_STRING(cpe_check, identifier)

/* <cpe-item><references><reference>
 * */
struct cpe_reference {
        struct xml_metadata xml;
	char *href;     // reference URL
	char *content;  // reference description
};
OSCAP_ACCESSOR_STRING(cpe_reference, href)
OSCAP_ACCESSOR_STRING(cpe_reference, content)

/* <generator>
 * */
struct cpe_generator {

        struct xml_metadata xml;
	char *product_name;    // generator software name
	char *product_version; // generator software version
	char *schema_version;	 // generator schema version
	char *timestamp;       // generation date and time
};
OSCAP_ACCESSOR_STRING(cpe_generator, product_name)
OSCAP_ACCESSOR_STRING(cpe_generator, product_version)
OSCAP_ACCESSOR_STRING(cpe_generator, schema_version)
OSCAP_ACCESSOR_STRING(cpe_generator, timestamp)

/* <cpe-list>
 * */
struct cpe_dict_model {                        // the main node

        struct xml_metadata xml;
	struct oscap_list* items;        // dictionary items
        struct oscap_list* vendors;
        struct cpe_generator* generator;
};
OSCAP_GETTER(struct cpe_generator*, cpe_dict_model, generator)
OSCAP_IGETTER_GEN(cpe_item, cpe_dict_model, items)
OSCAP_IGETINS_GEN(cpe_vendor, cpe_dict_model, vendors, vendor)

/* ****************************************
 * Component-tree structures
 * ***************************************/

/* vendor
 * */
struct cpe_vendor {
        struct xml_metadata xml;
        char *value;
        struct oscap_list* titles;
        struct oscap_list* products;
};
OSCAP_ACCESSOR_STRING(cpe_vendor, value)
OSCAP_IGETINS(oscap_title, cpe_vendor, titles, title)
OSCAP_IGETINS_GEN(cpe_product, cpe_vendor, products, product)

/* vendor -> product 
 * */
struct cpe_product {
        struct xml_metadata xml;
        char *value;
        cpe_part_t part;
        struct oscap_list* versions;
};
OSCAP_ACCESSOR_STRING(cpe_product, value)
OSCAP_ACCESSOR_SIMPLE(cpe_part_t, cpe_product, part)
OSCAP_IGETINS_GEN(cpe_version, cpe_product, versions, version)

/* vendor -> product -> version 
 * */
struct cpe_version {
        struct xml_metadata xml;
        char *value;
        struct oscap_list* updates;
};
OSCAP_ACCESSOR_STRING(cpe_version, value)
OSCAP_IGETINS_GEN(cpe_update, cpe_version, updates, update)

/* vendor -> product -> version -> update 
 * */
struct cpe_update {
        struct xml_metadata xml;
        char *value;
        struct oscap_list* editions;
};
OSCAP_ACCESSOR_STRING(cpe_update, value)
OSCAP_IGETINS_GEN(cpe_edition, cpe_update, editions, edition)

/* vendor -> product -> version -> update -> edition 
 * */
struct cpe_edition {
        struct xml_metadata xml;
        char *value;
        struct oscap_list* languages;
};
OSCAP_ACCESSOR_STRING(cpe_edition, value)
OSCAP_IGETINS_GEN(cpe_language, cpe_edition, languages, language)

/* vendor -> product -> version -> update -> edition -> language
 * */
struct cpe_language {
        struct xml_metadata xml;
        char *value;
};
OSCAP_ACCESSOR_STRING(cpe_language, value)

/* End of variable definitions
 * */
/***************************************************************************/

/***************************************************************************/
/* Declaration of static (private to this file) functions
 * These function shoud not be called from outside. For exporting these elements
 * has to call parent element's 
 */
static char * str_trim(char *str);
static int xmlTextReaderNextElement(xmlTextReaderPtr reader);
static bool cpe_dict_model_add_item(struct cpe_dict_model * dict, struct cpe_item * item);

static struct cpe_reference * cpe_reference_parse(xmlTextReaderPtr reader);
static struct cpe_check * cpe_check_parse(xmlTextReaderPtr reader);

static void cpe_product_export(const struct cpe_product * product, xmlTextWriterPtr writer);
static void cpe_version_export(const struct cpe_version * version, xmlTextWriterPtr writer);
static void cpe_update_export(const struct cpe_update * update, xmlTextWriterPtr writer);
static void cpe_edition_export(const struct cpe_edition * edition, xmlTextWriterPtr writer);
static void cpe_language_export(const struct cpe_language * language, xmlTextWriterPtr writer);
static void cpe_note_export(const struct oscap_title * title, xmlTextWriterPtr writer);
static void cpe_check_export(const struct cpe_check * check, xmlTextWriterPtr writer);
static void cpe_reference_export(const struct cpe_reference * ref, xmlTextWriterPtr writer);

void cpe_dict_model_free(struct cpe_dict_model * dict);
void cpe_generator_free(struct cpe_generator * generator);
void cpe_item_free(struct cpe_item * item);
void cpe_vendor_free(struct cpe_vendor * vendor);
void cpe_check_free(struct cpe_check * check);
void cpe_reference_free(struct cpe_reference * ref);
void cpe_product_free(struct cpe_product * product);
void cpe_version_free(struct cpe_version * version);
void cpe_update_free(struct cpe_update * update);
void cpe_edition_free(struct cpe_edition * edition);
void cpe_language_free(struct cpe_language * language);
void cpe_itemmetadata_free(struct cpe_item_metadata * meta);

/***************************************************************************/

/* Add item to dictionary. Function that just check both variables
 * on NULL value.
 * */
static bool cpe_dict_model_add_item(struct cpe_dict_model * dict, struct cpe_item * item) {

	if (dict == NULL || item == NULL)
		return false;

	oscap_list_add(dict->items, item);
	return true;
}

/* str_trim function that trims input string and returns string
 * without white characters as space or tabulator.
 * */
static char * str_trim(char *str) {
	int off, i;
	if (str == NULL)
		return NULL;
	for (i = 0; isspace(str[i]); ++i) ;
	off = i;
	while (str[i]) {
		str[i - off] = str[i];
		++i;
	}
	for (i -= off; isspace(str[--i]) && i >= 0;) ;
	str[++i] = '\0';
	return str;
}

/* Function that jump to next XML starting element.
 * */
static int xmlTextReaderNextElement(xmlTextReaderPtr reader) {

        int ret;
        do { 
              ret = xmlTextReaderRead(reader); 
              // if end of file
              if (ret == 0) break;
        } while (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT);
        return ret;
}

/***************************************************************************/
/* Constructors of CPE structures cpe_*<structure>*_new()
 * More info in representive header file.
 * returns the type of <structure>
 */
struct cpe_dict_model *cpe_dict_model_new() {

        struct cpe_dict_model *dict;

	dict = oscap_alloc(sizeof(struct cpe_dict_model));
	if (dict == NULL)
		return NULL;
	memset(dict, 0, sizeof(struct cpe_dict_model));

        dict->vendors   = oscap_list_new();
	dict->items     = oscap_list_new();

        dict->xml.lang      = NULL;
        dict->xml.namespace = NULL;

	return dict;
}

struct cpe_item *cpe_item_new() {

	struct cpe_item *item;

	item = oscap_alloc(sizeof(struct cpe_item));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_item));

	item->notes      = oscap_list_new();
	item->references = oscap_list_new();
	item->checks     = oscap_list_new();
        item->titles     = oscap_list_new();

        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_check *cpe_check_new() {

	struct cpe_check *item;

	item = oscap_alloc(sizeof(struct cpe_check));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_check));

	item->system        = NULL;
	item->href          = NULL;
        item->identifier    = NULL;

        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

        return item;
}

struct cpe_reference *cpe_reference_new() {

	struct cpe_reference *item;

	item = oscap_alloc(sizeof(struct cpe_reference));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_reference));

	item->href          = NULL;
        item->content       = NULL;

        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

        return item;
}

struct cpe_generator *cpe_generator_new() {

	struct cpe_generator *item;

	item = oscap_alloc(sizeof(struct cpe_generator));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_generator));

	item->product_name      = NULL;
	item->product_version   = NULL;
	item->schema_version    = NULL;
	item->timestamp         = NULL;

        item->xml.lang          = NULL;
        item->xml.namespace     = NULL;

        return item;
}

struct cpe_vendor *cpe_vendor_new() {

	struct cpe_vendor *item;

	item = oscap_alloc(sizeof(struct cpe_vendor));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_vendor));

        item->value      = NULL;
	item->titles     = oscap_list_new();
	item->products   = oscap_list_new();

        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_product *cpe_product_new() {

	struct cpe_product *item;

	item = oscap_alloc(sizeof(struct cpe_product));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_product));

        item->versions      = oscap_list_new();
        item->value         = NULL;
        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_version *cpe_version_new() {

	struct cpe_version *item;

	item = oscap_alloc(sizeof(struct cpe_version));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_version));

        item->updates       = oscap_list_new();
        item->value         = NULL;
        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_update *cpe_update_new() {

	struct cpe_update *item;

	item = oscap_alloc(sizeof(struct cpe_update));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_update));

        item->editions      = oscap_list_new();
        item->value         = NULL;
        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_edition *cpe_edition_new() {

	struct cpe_edition *item;

	item = oscap_alloc(sizeof(struct cpe_edition));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_edition));

        item->languages     = oscap_list_new();
        item->value         = NULL;
        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

struct cpe_language *cpe_language_new() {

	struct cpe_language *item;

	item = oscap_alloc(sizeof(struct cpe_language));
	if (item == NULL)
		return NULL;
	memset(item, 0, sizeof(struct cpe_language));

        item->value         = NULL;
        item->xml.lang      = NULL;
        item->xml.namespace = NULL;

	return item;
}

/* End of CPE structures' contructors
 * */
/***************************************************************************/

/***************************************************************************/
/* Private parsing functions cpe_*<structure>*_parse( xmlTextReaderPtr )
 * More info in representive header file.
 * returns the type of <structure>
 */
struct cpe_dict_model * cpe_dict_model_parse_xml(const struct oscap_import_source * source) {
        
    xmlTextReaderPtr reader;
    struct cpe_dict_model *dict = NULL;

    reader = xmlReaderForFile(oscap_import_source_get_filename(source), 
                              oscap_import_source_get_encoding(source), 0);
    if (reader != NULL) {
        xmlTextReaderRead(reader);
        dict = cpe_dict_model_parse(reader);
    } else {
        fprintf(stderr, "[CPE] Unable to open %s !\n", oscap_import_source_get_filename(source));
    }
    xmlFreeTextReader(reader);
    return dict;
}

struct cpe_dict_model * cpe_dict_model_parse(xmlTextReaderPtr reader) {

        struct cpe_dict_model *ret = NULL;
        struct cpe_item *item = NULL;
        struct cpe_vendor *vendor = NULL;
        int next_ret = 1;

        // let's find "<cpe-list>" element
        while (xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "cpe-list") && 
                (next_ret == 1)) {
                next_ret = xmlTextReaderNextElement(reader);
                // There is no "<cpe-list>" element :(( and we are at the end of file !
                if (next_ret == 0) {
                        fprintf(stderr, "There is no \"cpe-list\" element in the provided xml tree !\n");
                        return NULL;
                }
        }

        // we found cpe-list element, let's roll !
        // allocate memory for cpe_dict so we can fill items and vendors and general structures
        ret = cpe_dict_model_new();
        if (ret == NULL)
            return NULL;

        // TODO: here comes some header processing (for namespace definitions)


        // go through elements and switch through actions till end of file..
        next_ret = xmlTextReaderNextElement(reader);
        while (next_ret != 0) {
                
            if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "generator")) { // <generator> | count = 1
                        ret->generator = cpe_generator_parse(reader);
            } else 
                if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "cpe-item")) { // <cpe-item> | cout = 0-n
			if ((item = cpe_item_parse(reader)) == NULL) {
				// something bad happend, let's try to recover and continue
                                // add here some bad nodes list to write it to stdout after parsing is done
                                // get the next node
                                next_ret = xmlTextReaderNextElement(reader);
                                continue;
                        }
                        // We got an item !
			if (!cpe_dict_model_add_item(ret, item)) {
				cpe_item_free(item);
				cpe_dict_model_free(ret);
				return NULL;
			}
                        continue;
            } else
                if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "vendor")) { // <vendor> | count = 0-n
                        vendor = cpe_vendor_parse(reader);
			if (vendor) oscap_list_add(ret->vendors, vendor);
            } else
                // TODO: we need to store meta xml data of <component-tree> element
                if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "component-tree")) { // <vendor> | count = 0-n
                        // we just need to jump over this element
            }
            // get the next node
            next_ret = xmlTextReaderNextElement(reader);
        }

	return ret;
}

struct cpe_generator * cpe_generator_parse(xmlTextReaderPtr reader) {

        struct cpe_generator *ret = NULL;

        if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "generator") &&
            xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {

                // we are on "<generator>" element, let's alloc structure
	        ret = oscap_alloc(sizeof(struct cpe_generator));
                if (ret == NULL)
                        return NULL;

                ret->xml.lang = (char *) xmlTextReaderConstXmlLang(reader);
                ret->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                
                // skip nodes until new element
                xmlTextReaderNextElement(reader);

                while (xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "generator") != 0) {

                        if ((xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                            (const xmlChar *)"product_name") == 0) &&
                            (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT)){
			            ret->product_name = (char *) xmlTextReaderReadString(reader);
                        } else 
                            if ((xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"product_version") == 0) &&
                                (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT)){
			            ret->product_version = (char *) xmlTextReaderReadString(reader);
                        } else 
                            if ((xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"schema_version") == 0) &&
                                (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT)){
			            ret->schema_version = (char *) xmlTextReaderReadString(reader);
                        } else 
                            if ((xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"timestamp") == 0) &&
                                (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT)){
			            ret->timestamp = (char *) xmlTextReaderReadString(reader);
                        }

                        // element saved. Let's jump on the very next one node (not element, because we need to 
                        // find XML_READER_TYPE_END_ELEMENT node, see "while" condition and the condition below "while"
                        xmlTextReaderRead(reader);

                }
                // we found another element generator which is not "end element" ? Horrible !
                if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) 
                        // print some nasty error message ?
                        return NULL;
        }

        return ret;

}


struct cpe_item * cpe_item_parse(xmlTextReaderPtr reader) {

        struct cpe_item *ret = NULL;
        struct oscap_title *title = NULL;
        struct cpe_check *check = NULL;
        struct cpe_reference *ref = NULL;
        char *data;

        if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), BAD_CAST "cpe-item") &&
            xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {

                // we are on "<cpe-item>" element, let's alloc structure
                ret = cpe_item_new();
                if (ret == NULL)
                        return NULL;

                ret->xml.lang = (char *) xmlTextReaderConstXmlLang(reader);
                ret->xml.namespace = (char *) xmlTextReaderPrefix(reader);

                // Get a name attribute of cpe-item
                data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "name");
                if (data != NULL) 
                    ret->name = cpe_name_new(data);
                oscap_free(data);

                // if there is "deprecated", "deprecated_by" and "deprecation_date" in cpe-item element
                // ************************************************************************************
                data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "deprecated");
                if (data != NULL) { // we have a deprecation here !
	                oscap_free(data);
                        data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "deprecated_by");
	                if (data == NULL || (ret->deprecated = cpe_name_new(data)) == NULL) {
		                oscap_free(ret);
		                oscap_free(data);
		                return NULL;
                        }
	                oscap_free(data);

                        data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "deprecation_date");
	                if(data == NULL || (ret->deprecation_date = malloc(strlen(data)+1)) == NULL)   {
		                oscap_free(ret);
		                oscap_free(data);
		                return NULL;
                        }
                        strcpy(ret->deprecation_date, (char *)data);
                }
                oscap_free(data);
                // ************************************************************************************

                xmlTextReaderNextElement(reader);
                // Now it's time to go deaply to cpe-item element and parse it's children
                // Do while there is another cpe-item element. Then return.
                while (xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "cpe-item") != 0) {
                        
                        if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT) {
                                xmlTextReaderRead(reader);
                                continue;
                        }

                        if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                            (const xmlChar *)"title") == 0) {
                                    title = oscap_title_parse(reader, "title");
                                    if (title) oscap_list_add(ret->titles, title);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"note") == 0) {
                                    // it's OK to use title varible here, because note is the same structure,
                                    // not the reason to make a new same one
                                    title = oscap_title_parse(reader, "note");
                                    if (title) oscap_list_add(ret->notes, title);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"check") == 0) {
                                    check = cpe_check_parse(reader);
                                    if (check) oscap_list_add(ret->checks, check);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"reference") == 0) {
                                    ref = cpe_reference_parse(reader);
                                    if (ref) oscap_list_add(ret->references, ref);
                                    if (ref) printf("ref: %s\n", ref->href);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"item-metadata") == 0) {
                                    data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "modification-date");
                                    if ((data == NULL) || ((ret->metadata = oscap_alloc(sizeof(struct cpe_item_metadata))) == NULL)){
                                            cpe_item_free(ret);
                                            oscap_free(data);
                                            return NULL;
                                    }
                                    ret->metadata->modification_date = data;
                                    ret->metadata->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    ret->metadata->xml.namespace = (char *) xmlTextReaderPrefix(reader);

                                    data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "status");
	                            if (data) ret->metadata->status = data;
                                    data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "nvd-id");
	                            if (data) ret->metadata->nvd_id = (char *)data;
                                    data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "deprecated-by-nvd-id");
	                            if (data) ret->metadata->deprecated_by_nvd_id = (char *)data;
                                    else ret->metadata->deprecated_by_nvd_id = NULL;
                                    data = NULL;

                        } else 
                            if ((xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"references") == 0) || 
                                (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"notes") == 0)) {
                                // we just need to jump over this element
                        
                        } else {
                                return ret; // <-- we need to return here, because we don't want to jump to next element 
                        }
                        xmlTextReaderNextElement(reader);
                }
        }

        return ret;
}

static struct cpe_check * cpe_check_parse(xmlTextReaderPtr reader) {

	struct cpe_check *ret;

        if (xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "check") != 0)
		return NULL;

	if ((ret = oscap_alloc(sizeof(struct cpe_check))) == NULL)
		return NULL;
	memset(ret, 0, sizeof(struct cpe_check));

        ret->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
        ret->xml.namespace = (char *) xmlTextReaderPrefix(reader);
        ret->system = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "system");
        ret->href = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "href");
	ret->identifier = str_trim((char *) xmlTextReaderReadString(reader));

	return ret;
}

static struct cpe_reference * cpe_reference_parse(xmlTextReaderPtr reader) {

	struct cpe_reference *ret;

        if (xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "reference") != 0)
		return NULL;

	if ((ret = oscap_alloc(sizeof(struct cpe_reference))) == NULL)
		return NULL;
	memset(ret, 0, sizeof(struct cpe_reference));

        ret->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
        ret->xml.namespace = (char *) xmlTextReaderPrefix(reader);
        ret->href = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "href");
	ret->content = str_trim((char *) xmlTextReaderReadString(reader));

	return ret;
}

struct cpe_vendor * cpe_vendor_parse(xmlTextReaderPtr reader) {


        struct cpe_vendor *ret         = NULL;
        struct oscap_title *title    = NULL;
        struct cpe_product *product    = NULL;
        struct cpe_version *version    = NULL;
        struct cpe_update *update      = NULL;
        struct cpe_edition *edition    = NULL;
        struct cpe_language *language  = NULL;
        char *data;

        if (xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "vendor") != 0)
		return NULL;

        ret = cpe_vendor_new();
        if (ret == NULL)
                return NULL;

        ret->xml.namespace = (char *) xmlTextReaderPrefix(reader);
        ret->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");
        // jump to next element (which should be product)
        xmlTextReaderNextElement(reader);

        while(xmlStrcmp (xmlTextReaderConstLocalName(reader), BAD_CAST "vendor") != 0) {
                        
                        if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT) {
                                xmlTextReaderRead(reader);
                                continue;
                        }

                        if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                            (const xmlChar *)"title") == 0) {
                                    title = oscap_title_parse(reader, "title");
                                    if (title) oscap_list_add(ret->titles, title);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"product") == 0) {
                                    // initialization
                                    product = cpe_product_new();
                                    product->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    product->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                                    product->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");

	                            data = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "part");
	                            if (data) {
	                                    if (strcasecmp((const char *)data, "h") == 0)
		                                    product->part = CPE_PART_HW;
	                                    else if (strcasecmp((const char *)data, "o") == 0)
		                                    product->part = CPE_PART_OS;
	                                    else if (strcasecmp((const char *)data, "a") == 0)
		                                    product->part = CPE_PART_APP;
	                                    else {
		                                    oscap_free(ret);
		                                    oscap_free(data);
	                                            return NULL;
                                            }
	                            } else {
                                            product->part = CPE_PART_NONE;
                                    }
                                    oscap_free(data);

                                    if (product) oscap_list_add(ret->products, product);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"version") == 0) {
                                    // initialization
                                    version = cpe_version_new();
                                    version->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    version->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                                    version->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");
                                    oscap_list_add(product->versions, version);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"update") == 0) {
                                    // initialization
                                    update = cpe_update_new();
                                    update->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    update->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                                    update->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");
                                    oscap_list_add(version->updates, update);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"edition") == 0) {
                                    // initialization
                                    edition = cpe_edition_new();
                                    edition->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    edition->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                                    edition->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");
                                    oscap_list_add(update->editions, edition);
                        } else 
                            if (xmlStrcmp(xmlTextReaderConstLocalName(reader), 
                                (const xmlChar *)"language") == 0) {
                                    // initialization
                                    language = cpe_language_new();
                                    language->xml.lang = oscap_strdup((char *) xmlTextReaderConstXmlLang(reader));
                                    language->xml.namespace = (char *) xmlTextReaderPrefix(reader);
                                    language->value = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "value");
                                    oscap_list_add(edition->languages, language);
                        }
                        xmlTextReaderRead(reader);
        }
        return ret;
        
}

/* End of private parsing functions
 * */
/***************************************************************************/

/***************************************************************************/
/* Private exporting functions cpe_*<structure>*_export( xmlTextWriterPtr )
 * More info in representive header file.
 * returns the type of <structure>
 */
void cpe_dict_model_export(struct cpe_dict_model * dict, const struct oscap_export_target * target) {

        // TODO: add macro to check return value from xmlTextWriter* functions
        xmlTextWriterPtr writer;

        writer = xmlNewTextWriterFilename(oscap_export_target_get_filename(target), 0);

        // Set properties of writer TODO: make public function to edit this ??
        // Yes - there will be structure oscap_export_target & oscap_parse_target
        xmlTextWriterSetIndent(writer, oscap_export_target_get_indent(target));
        xmlTextWriterSetIndentString(writer, BAD_CAST oscap_export_target_get_indent_string(target));

        xmlTextWriterStartDocument(writer, NULL, oscap_export_target_get_encoding(target), NULL);

        cpe_dict_export(dict, writer);
        cpe_dict_model_free(dict);
        xmlTextWriterEndDocument(writer);
        xmlFreeTextWriter(writer);
}

void cpe_dict_export(const struct cpe_dict_model * dict, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST dict->xml.namespace, BAD_CAST "cpe-list", NULL);
        // TODO: this shouldn't be hardcoded - find another way !
        xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns", BAD_CAST 
                "http://cpe.mitre.org/dictionary/2.0");
        xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:xsi", BAD_CAST 
                "http://www.w3.org/2001/XMLSchema-instance");
        xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:meta", BAD_CAST 
                "http://scap.nist.gov/schema/cpe-dictionary-metadata/0.2");
        xmlTextWriterWriteAttribute(writer, BAD_CAST "xsi:schemaLocation", BAD_CAST 
                "http://scap.nist.gov/schema/cpe-dictionary-metadata/0.2 http://nvd.nist.gov/schema/cpe-dictionary-metadata_0.2.xsd http://cpe.mitre.org/dictionary/2.0 http://cpe.mitre.org/files/cpe-dictionary_2.1.xsd");
        if (dict->generator != NULL) {
            cpe_generator_export( dict->generator, writer);
        }
        OSCAP_FOREACH (cpe_item, item, cpe_dict_model_get_items(dict),
		// dump its contents to XML tree
                cpe_item_export( item, writer );
	)
        // TODO: NEED TO HAVE COMPONENT-TREE STRUCTURE TO GET XML-NAMESPACE 
        xmlTextWriterStartElementNS(writer, BAD_CAST "meta", BAD_CAST "component-tree", NULL);
        OSCAP_FOREACH (cpe_vendor, vendor, cpe_dict_model_get_vendors(dict),
                cpe_vendor_export( vendor, writer );
	)
        xmlTextWriterEndElement(writer);//</component-tree>

        xmlTextWriterEndElement(writer);
}

void cpe_generator_export(const struct cpe_generator * generator, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST generator->xml.namespace, BAD_CAST "generator", NULL);
        if (generator->product_name != NULL) {
                xmlTextWriterStartElementNS(writer, BAD_CAST generator->xml.namespace, BAD_CAST "product_name", NULL);
                xmlTextWriterWriteString(writer, BAD_CAST generator->product_name);
                xmlTextWriterEndElement(writer); //</product_name>
        }
        if (generator->product_version != NULL) {
                xmlTextWriterStartElementNS(writer, BAD_CAST generator->xml.namespace, BAD_CAST "product_version", NULL);
                xmlTextWriterWriteString(writer, BAD_CAST generator->product_version);
                xmlTextWriterEndElement(writer); //</product_name>
        }
        if (generator->schema_version != NULL) {
                xmlTextWriterStartElementNS(writer, BAD_CAST generator->xml.namespace, BAD_CAST "schema_version", NULL);
                xmlTextWriterWriteString(writer, BAD_CAST generator->schema_version);
                xmlTextWriterEndElement(writer); //</product_name>
        }
        if (generator->timestamp != NULL) {
                xmlTextWriterStartElementNS(writer, BAD_CAST generator->xml.namespace, BAD_CAST "timestamp", NULL);
                xmlTextWriterWriteString(writer, BAD_CAST generator->timestamp);
                xmlTextWriterEndElement(writer); //</product_name>
        }
        xmlTextWriterEndElement(writer); //</gnerator>

}

void cpe_item_export(const struct cpe_item * item, xmlTextWriterPtr writer) {

        char *temp;
        struct oscap_iterator *it;;

        xmlTextWriterStartElementNS(writer, BAD_CAST item->xml.namespace, BAD_CAST "cpe-item", NULL);
        if (item->name != NULL) {
                temp = cpe_name_get_uri(item->name);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "name", BAD_CAST temp);
                oscap_free(temp);
        }
        if (item->deprecated != NULL) {
                temp = cpe_name_get_uri(item->deprecated);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "deprecated", BAD_CAST "true");
                xmlTextWriterWriteAttribute(writer, BAD_CAST "deprecation_date", BAD_CAST item->deprecation_date);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "deprecated_by", BAD_CAST temp);
                oscap_free(temp);
        }
        
        OSCAP_FOREACH (oscap_title, title, cpe_item_get_titles(item),
                oscap_title_export( title, writer);
	)

        if (item->metadata != NULL) {
                xmlTextWriterStartElementNS(writer, BAD_CAST item->metadata->xml.namespace, BAD_CAST "item-metadata", NULL);
                if (item->metadata->modification_date != NULL)
                        xmlTextWriterWriteAttribute(writer, BAD_CAST "modification-date", BAD_CAST item->metadata->modification_date);
                if (item->metadata->status != NULL)
                        xmlTextWriterWriteAttribute(writer, BAD_CAST "status", BAD_CAST item->metadata->status);
                if (item->metadata->nvd_id != NULL)
                        xmlTextWriterWriteAttribute(writer, BAD_CAST "nvd-id", BAD_CAST item->metadata->nvd_id);
                if (item->metadata->deprecated_by_nvd_id != NULL)
                        xmlTextWriterWriteAttribute(writer, BAD_CAST "deprecated-by-nvd-id", BAD_CAST item->metadata->deprecated_by_nvd_id);
                xmlTextWriterEndElement(writer);
        }

        it = oscap_iterator_new(item->references);
        if (oscap_iterator_has_more(it)) {
                xmlTextWriterStartElementNS(writer, NULL, BAD_CAST "references", NULL);
                OSCAP_FOREACH (cpe_reference, ref, cpe_item_get_references(item), 
                        cpe_reference_export(ref, writer); 
                )
                xmlTextWriterEndElement(writer);
        }
        oscap_iterator_free(it);

        it = oscap_iterator_new(item->notes);
        if (oscap_iterator_has_more(it)) {
                xmlTextWriterStartElementNS(writer, NULL, BAD_CAST "notes", NULL);
                OSCAP_FOREACH (oscap_title, note, cpe_item_get_notes(item), 
                        cpe_note_export(note, writer); 
                )
                xmlTextWriterEndElement(writer);
        }
        oscap_iterator_free(it);

        OSCAP_FOREACH (cpe_check, check, cpe_item_get_checks(item), 
                cpe_check_export(check, writer); 
        )

        xmlTextWriterEndElement(writer);

}

void cpe_vendor_export(const struct cpe_vendor * vendor, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST vendor->xml.namespace, BAD_CAST "vendor", NULL);
        if (vendor->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST vendor->value);

        OSCAP_FOREACH (oscap_title, title, cpe_vendor_get_titles(vendor),
                oscap_title_export( title, writer);
	)

        OSCAP_FOREACH (cpe_product, product, cpe_vendor_get_products(vendor),
                cpe_product_export(product, writer);
	)

        xmlTextWriterEndElement(writer);
}

static void cpe_product_export(const struct cpe_product * product, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST product->xml.namespace, BAD_CAST "product", NULL);
        if (product->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST product->value);
        if (product->part != CPE_PART_NONE)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "part", BAD_CAST PART_TO_CHAR[product->part] );

        OSCAP_FOREACH (cpe_version, version, cpe_product_get_versions(product),
                cpe_version_export(version, writer);
	)

        xmlTextWriterEndElement(writer);
}

static void cpe_version_export(const struct cpe_version * version, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST version->xml.namespace, BAD_CAST "version", NULL);
        if (version->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST version->value);

        OSCAP_FOREACH (cpe_update, update, cpe_version_get_updates(version),
                cpe_update_export(update, writer);
	)

        xmlTextWriterEndElement(writer);

}

static void cpe_update_export(const struct cpe_update * update, xmlTextWriterPtr writer){ 

        xmlTextWriterStartElementNS(writer, BAD_CAST update->xml.namespace, BAD_CAST "update", NULL);
        if (update->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST update->value);

        OSCAP_FOREACH (cpe_edition, edition, cpe_update_get_editions(update),
                cpe_edition_export(edition, writer);
	)

        xmlTextWriterEndElement(writer);
}
static void cpe_edition_export(const struct cpe_edition * edition, xmlTextWriterPtr writer){

        xmlTextWriterStartElementNS(writer, BAD_CAST edition->xml.namespace, BAD_CAST "edition", NULL);
        if (edition->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST edition->value);

        OSCAP_FOREACH (cpe_language, language, cpe_edition_get_languages(edition),
                cpe_language_export(language, writer);
	)

        xmlTextWriterEndElement(writer);
}
static void cpe_language_export(const struct cpe_language * language, xmlTextWriterPtr writer){

        xmlTextWriterStartElementNS(writer, BAD_CAST language->xml.namespace, BAD_CAST "language", NULL);
        if (language->value != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "value", BAD_CAST language->value);
        if (language->xml.lang != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "xml:lang", BAD_CAST language->xml.lang);

        xmlTextWriterEndElement(writer);
}

static void cpe_note_export(const struct oscap_title * title, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST title->xml.namespace, BAD_CAST "note", NULL);
        if (title->xml.lang != NULL) 
                xmlTextWriterWriteAttribute(writer, BAD_CAST "xml:lang", BAD_CAST title->xml.lang);
        if (title->content != NULL) 
                xmlTextWriterWriteString(writer, BAD_CAST title->content);
        xmlTextWriterEndElement(writer);
}
static void cpe_check_export(const struct cpe_check *check, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST check->xml.namespace, BAD_CAST "check", NULL);
        if (check->system != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "system", BAD_CAST check->system);
        if (check->href != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "href", BAD_CAST check->href);
        if (check->identifier != NULL)
                xmlTextWriterWriteString(writer, BAD_CAST check->identifier);
        xmlTextWriterEndElement(writer);
}
static void cpe_reference_export(const struct cpe_reference * ref, xmlTextWriterPtr writer) {

        xmlTextWriterStartElementNS(writer, BAD_CAST ref->xml.namespace, BAD_CAST "reference", NULL);
        if (ref->href != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "href", BAD_CAST ref->href);
        if (ref->content != NULL)
                xmlTextWriterWriteString(writer, BAD_CAST ref->content);
        if (ref->xml.lang != NULL)
                xmlTextWriterWriteAttribute(writer, BAD_CAST "xml:lang", BAD_CAST ref->xml.lang);
        xmlTextWriterEndElement(writer);
}
/* End of private export functions
 * */
/***************************************************************************/

/***************************************************************************/
/* Free functions - all are static private, do not use them outside this file
 */
void cpe_dict_model_free(struct cpe_dict_model * dict) {

        if (dict == NULL) return;

	oscap_list_free(dict->items, (oscap_destruct_func)cpe_item_free);
	oscap_list_free(dict->vendors, (oscap_destruct_func)cpe_vendor_free);
	cpe_generator_free(dict->generator);
        xml_metadata_free(dict->xml);
	oscap_free(dict);
}

void cpe_item_free(struct cpe_item * item) {

	if (item == NULL) return;
	cpe_name_free(item->name);
	cpe_name_free(item->deprecated);
	oscap_free(item->deprecation_date);
	oscap_list_free(item->references, (oscap_destruct_func)cpe_reference_free);
	oscap_list_free(item->checks, (oscap_destruct_func)cpe_check_free);
	oscap_list_free(item->notes, (oscap_destruct_func)oscap_title_free);
        oscap_list_free(item->titles, (oscap_destruct_func)oscap_title_free);
        cpe_itemmetadata_free(item->metadata);
        xml_metadata_free(item->xml);
	oscap_free(item);
}

void cpe_generator_free(struct cpe_generator * generator) {

	if (generator == NULL) return;

	oscap_free(generator->product_name);
	oscap_free(generator->product_version);
	oscap_free(generator->schema_version);
	oscap_free(generator->timestamp);
        xml_metadata_free(generator->xml);
	oscap_free(generator);
}

void cpe_check_free(struct cpe_check * check) {

	if (check == NULL) return;

	oscap_free(check->identifier);
	oscap_free(check->system);
	oscap_free(check->href);
        xml_metadata_free(check->xml);
	oscap_free(check);
}

void cpe_reference_free(struct cpe_reference* ref) {

	if (ref == NULL) return;

	oscap_free(ref->href);
	oscap_free(ref->content);
        xml_metadata_free(ref->xml);
	oscap_free(ref);
}

void cpe_vendor_free(struct cpe_vendor * vendor) {

	if (vendor == NULL) return;

        oscap_free(vendor->value);
	oscap_list_free(vendor->titles, (oscap_destruct_func)oscap_title_free);
	oscap_list_free(vendor->products, (oscap_destruct_func)cpe_product_free);
        xml_metadata_free(vendor->xml);
	oscap_free(vendor);
}

void cpe_product_free(struct cpe_product * product) {

	if (product == NULL) return;

	oscap_free(product->value);
	oscap_list_free(product->versions, (oscap_destruct_func)cpe_version_free);
        xml_metadata_free(product->xml);
	oscap_free(product);
}

void cpe_version_free(struct cpe_version * version) {

	if (version == NULL) return;

	oscap_free(version->value);
	oscap_list_free(version->updates, (oscap_destruct_func)cpe_update_free);
        xml_metadata_free(version->xml);
	oscap_free(version);
}

void cpe_update_free(struct cpe_update * update) {

	if (update == NULL) return;

	oscap_free(update->value);
	oscap_list_free(update->editions, (oscap_destruct_func)cpe_edition_free);
        xml_metadata_free(update->xml);
	oscap_free(update);
}

void cpe_edition_free(struct cpe_edition * edition) {
    
	if (edition == NULL) return;

	oscap_free(edition->value);
	oscap_list_free(edition->languages, (oscap_destruct_func)cpe_language_free);
        xml_metadata_free(edition->xml);
	oscap_free(edition);
}

void cpe_language_free(struct cpe_language * language) {
    
	if (language == NULL) return;

	oscap_free(language->value);
        xml_metadata_free(language->xml);
	oscap_free(language);
}

void cpe_itemmetadata_free(struct cpe_item_metadata * meta) {

        if (meta == NULL) return;

        oscap_free(meta->modification_date);
        oscap_free(meta->status);
        oscap_free(meta->nvd_id);
        oscap_free(meta->deprecated_by_nvd_id);
        xml_metadata_free(meta->xml);
        oscap_free(meta);
}
/* End of free functions
 * */
/***************************************************************************/
