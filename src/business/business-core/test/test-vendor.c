#include <glib.h>
#include <guile/gh.h>

#include "guid.h"
#include "gnc-module.h"
#include "gnc-engine-util.h"

#include "gncBusiness.h"
#include "gncVendor.h"
#include "gncVendorP.h"
#include "test-stuff.h"

static int count = 0;

static void
test_string_fcn (GncBusiness *bus, const char *message,
		 void (*set) (GncVendor *, const char *str),
		 const char * (*get)(GncVendor *));

static void
test_numeric_fcn (GncBusiness *bus, const char *message,
		  void (*set) (GncVendor *, gnc_numeric),
		  gnc_numeric (*get)(GncVendor *));

static void
test_bool_fcn (GncBusiness *bus, const char *message,
		  void (*set) (GncVendor *, gboolean),
		  gboolean (*get) (GncVendor *));

static void
test_gint_fcn (GncBusiness *bus, const char *message,
	       void (*set) (GncVendor *, gint),
	       gint (*get) (GncVendor *));

static void
test_vendor (void)
{
  GncBusiness *bus;
  GncVendor *vendor;

  bus = gncBusinessCreate ((GNCBook *)1);

  /* Test creation/destruction */
  {
    do_test (gncVendorCreate (NULL) == NULL, "vendor create NULL");
    vendor = gncVendorCreate (bus);
    do_test (vendor != NULL, "vendor create");
    do_test (gncVendorGetBusiness (vendor) == bus,
	     "getbusiness");

    gncVendorDestroy (vendor);
    success ("create/destroy");
  }

  /* Test setting/getting routines; does the active flag get set right? */
  {
    GUID guid;

    test_string_fcn (bus, "Id", gncVendorSetID, gncVendorGetID);
    test_string_fcn (bus, "Name", gncVendorSetName, gncVendorGetName);
    test_string_fcn (bus, "Notes", gncVendorSetNotes, gncVendorGetNotes);

    test_gint_fcn (bus, "Terms", gncVendorSetTerms, gncVendorGetTerms);

    test_bool_fcn (bus, "TaxIncluded", gncVendorSetTaxIncluded, gncVendorGetTaxIncluded);
    test_bool_fcn (bus, "Active", gncVendorSetActive, gncVendorGetActive);

    do_test (gncVendorGetAddr (vendor) != NULL, "Addr");

    guid_new (&guid);
    vendor = gncVendorCreate (bus); count++;
    gncVendorSetGUID (vendor, &guid);
    do_test (guid_equal (&guid, gncVendorGetGUID (vendor)), "guid compare");
  }
  {
    GList *list;

    list = gncBusinessGetList (bus, GNC_VENDOR_MODULE_NAME, TRUE);
    do_test (list != NULL, "getList all");
    do_test (g_list_length (list) == count, "correct length: all");
    g_list_free (list);

    list = gncBusinessGetList (bus, GNC_VENDOR_MODULE_NAME, FALSE);
    do_test (list != NULL, "getList active");
    do_test (g_list_length (list) == 1, "correct length: active");
    g_list_free (list);
  }
  {
    const char *str = get_random_string();
    const char *res;

    gncVendorSetName (vendor, str);
    res = gncBusinessPrintable (bus, GNC_VENDOR_MODULE_NAME, vendor);
    do_test (res != NULL, "Printable NULL?");
    do_test (safe_strcmp (str, res) == 0, "Printable equals");
  }    
}

static void
test_string_fcn (GncBusiness *bus, const char *message,
		 void (*set) (GncVendor *, const char *str),
		 const char * (*get)(GncVendor *))
{
  GncVendor *vendor = gncVendorCreate (bus);
  char const *str = get_random_string ();

  do_test (!gncVendorIsDirty (vendor), "test if start dirty");
  set (vendor, str);
  do_test (gncVendorIsDirty (vendor), "test dirty later");
  do_test (safe_strcmp (get (vendor), str) == 0, message);
  gncVendorSetActive (vendor, FALSE); count++;
}

static void
test_numeric_fcn (GncBusiness *bus, const char *message,
		  void (*set) (GncVendor *, gnc_numeric),
		  gnc_numeric (*get)(GncVendor *))
{
  GncVendor *vendor = gncVendorCreate (bus);
  gnc_numeric num = gnc_numeric_create (17, 1);

  do_test (!gncVendorIsDirty (vendor), "test if start dirty");
  set (vendor, num);
  do_test (gncVendorIsDirty (vendor), "test dirty later");
  do_test (gnc_numeric_equal (get (vendor), num), message);
  gncVendorSetActive (vendor, FALSE); count++;
}

static void
test_bool_fcn (GncBusiness *bus, const char *message,
	       void (*set) (GncVendor *, gboolean),
	       gboolean (*get) (GncVendor *))
{
  GncVendor *vendor = gncVendorCreate (bus);
  gboolean num = get_random_boolean ();

  do_test (!gncVendorIsDirty (vendor), "test if start dirty");
  set (vendor, FALSE);
  set (vendor, TRUE);
  set (vendor, num);
  do_test (gncVendorIsDirty (vendor), "test dirty later");
  do_test (get (vendor) == num, message);
  gncVendorSetActive (vendor, FALSE); count++;
}

static void
test_gint_fcn (GncBusiness *bus, const char *message,
	       void (*set) (GncVendor *, gint),
	       gint (*get) (GncVendor *))
{
  GncVendor *vendor = gncVendorCreate (bus);
  gint num = 17;

  do_test (!gncVendorIsDirty (vendor), "test if start dirty");
  set (vendor, num);
  do_test (gncVendorIsDirty (vendor), "test dirty later");
  do_test (get (vendor) == num, message);
  gncVendorSetActive (vendor, FALSE); count++;
}

static void
main_helper (int argc, char **argv)
{
  gnc_module_load("gnucash/business-core", 0);
  test_vendor();
  print_test_results();
  exit(get_rv());
}

int
main (int argc, char **argv)
{
  gh_enter (argc, argv, main_helper);
  return 0;
}