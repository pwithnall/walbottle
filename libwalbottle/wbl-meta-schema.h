/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2015 <philip@tecnocode.co.uk>
 *
 * Walbottle is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Walbottle is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Walbottle.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WBL_META_SCHEMA_H
#define WBL_META_SCHEMA_H

#include <glib.h>
#include <gio/gio.h>

#include "wbl-schema.h"

G_BEGIN_DECLS

/**
 * WblMetaSchemaType:
 * @WBL_META_SCHEMA_META_SCHEMA: Meta-schema used for schemas written for pure
 *   validation.
 * @WBL_META_SCHEMA_HYPER_META_SCHEMA: Meta-schema used for schemas written for
 *   validation and hyper-linking.
 *
 * Identifiers for the different meta-schemas available for validating JSON
 * schemas.
 *
 * Since: UNRELEASED
 */
typedef enum {
	WBL_META_SCHEMA_META_SCHEMA,
	WBL_META_SCHEMA_HYPER_META_SCHEMA,
} WblMetaSchemaType;

GInputStream *
wbl_meta_schema_load (WblMetaSchemaType meta_schema_type,
                      GError **error);
WblSchema *
wbl_meta_schema_load_schema (WblMetaSchemaType meta_schema_type,
                             GError **error);

G_END_DECLS

#endif /* !WBL_META_SCHEMA_H */
