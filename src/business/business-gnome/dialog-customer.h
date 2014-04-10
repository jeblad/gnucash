/*
 * dialog-customer.h -- Dialog(s) for Customer search and entry
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */


#ifndef GNC_DIALOG_CUSTOMER_H_
#define GNC_DIALOG_CUSTOMER_H_

/* Functions to create and edit customers */
GncCustomer * gnc_customer_new (GtkWidget *parent, GncBusiness *bus);
void gnc_customer_edit (GtkWidget *parent, GncCustomer *cust);

/* Callbacks to select a customer that match the necessary functions
 * for use with the gnc_general_select widget.
 *
 * new_select provides a selection and the ability to create and edit
 *	customers.
 * new_edit provides only the ability to edit the current selection
 */
gpointer        gnc_customer_edit_new_select (gpointer bus, gpointer c,
					      GtkWidget *toplevel);
gpointer	gnc_customer_edit_new_edit (gpointer bus, gpointer cust,
					    GtkWidget *toplevel);

#endif /* GNC_DIALOG_CUSTOMER_H_ */