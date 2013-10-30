#include <DB/Parsers/ASTFunction.h>
#include <DB/Parsers/ASTIdentifier.h>
#include <DB/Parsers/ASTNameTypePair.h>
#include <DB/Parsers/ASTExpressionList.h>
#include <DB/Parsers/ASTCreateQuery.h>

#include <DB/Parsers/CommonParsers.h>
#include <DB/Parsers/ExpressionListParsers.h>
#include <DB/Parsers/ParserCreateQuery.h>
#include <DB/Parsers/ParserSelectQuery.h>


namespace DB
{
	
bool ParserNestedTable::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	ParserWhiteSpaceOrComments ws;
	ParserString open("(");
	ParserString close(")");
	ParserIdentifier name_p;
	ParserNameTypePairList columns_p;
	
	ASTPtr name;
	ASTPtr columns;
	
	Pos begin = pos;
	
	/// Пока name == 'Nested', возможно потом появятся альтернативные вложенные структуры данных
	if (!name_p.parse(pos, end, name, expected))
		return false;
	
	ws.ignore(pos, end);
	
	if (!open.ignore(pos, end))
		return false;
	
	ws.ignore(pos, end);
	
	if (!columns_p.parse(pos, end, columns, expected))
		return false;
	
	ws.ignore(pos, end);
	
	if (!close.ignore(pos, end))
		return false;
	
	ASTFunction * func = new ASTFunction(StringRange(begin, pos));
	node = func;
	func->name = dynamic_cast<ASTIdentifier &>(*name).name;
	func->arguments = columns;
	func->children.push_back(columns);
	
	return true;
}
	
	
bool ParserIdentifierWithParameters::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	Pos begin = pos;
	
	ParserFunction function_or_array;
	if (function_or_array.parse(pos, end, node, expected))
		return true;
	
	pos = begin;
	
	ParserNestedTable nested;	
	if (nested.parse(pos, end, node, expected))
		return true;
	
	pos = begin;
	
	return false;
}


bool ParserIdentifierWithOptionalParameters::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	ParserIdentifier non_parametric;
	ParserIdentifierWithParameters parametric;
	
	Pos begin = pos;

	if (parametric.parse(pos, end, node, expected))
	{
		return true;
	}
	pos = begin;

	ASTPtr ident;
	if (non_parametric.parse(pos, end, ident, expected))
	{
		ASTFunction * func = new ASTFunction(StringRange(begin, pos));
		node = func;
		func->name = dynamic_cast<ASTIdentifier &>(*ident).name;
		return true;
	}
	pos = begin;

	return false;
}


bool ParserNameTypePair::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	ParserIdentifier name_parser;
	ParserIdentifierWithOptionalParameters type_parser;
	ParserWhiteSpaceOrComments ws_parser;
	
	Pos begin = pos;

	ASTPtr name, type;
	if (name_parser.parse(pos, end, name, expected)
		&& ws_parser.ignore(pos, end, expected)
		&& type_parser.parse(pos, end, type, expected))
	{
		ASTNameTypePair * name_type_pair = new ASTNameTypePair(StringRange(begin, pos));
		node = name_type_pair;
		name_type_pair->name = dynamic_cast<ASTIdentifier &>(*name).name;
		name_type_pair->type = type;
		name_type_pair->children.push_back(type);
		return true;
	}
	
	pos = begin;
	return false;
}


bool ParserNameTypePairList::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	return ParserList(new ParserNameTypePair, new ParserString(","), false).parse(pos, end, node, expected);
}


bool ParserEngine::parseImpl(Pos & pos, Pos end, ASTPtr & storage, String & expected)
{
	ParserWhiteSpaceOrComments ws;
	ParserString s_engine("ENGINE", true, true);
	ParserString s_eq("=");
	ParserIdentifierWithOptionalParameters storage_p;

	ws.ignore(pos, end);
	
	if (s_engine.ignore(pos, end, expected))
	{
		ws.ignore(pos, end);

		if (!s_eq.ignore(pos, end, expected))
			return false;

		ws.ignore(pos, end);

		if (!storage_p.parse(pos, end, storage, expected))
			return false;

		ws.ignore(pos, end);
	}

	return true;
}


bool ParserCreateQuery::parseImpl(Pos & pos, Pos end, ASTPtr & node, String & expected)
{
	Pos begin = pos;

	ParserWhiteSpaceOrComments ws;
	ParserString s_create("CREATE", true, true);
	ParserString s_attach("ATTACH", true, true);
	ParserString s_table("TABLE", true, true);
	ParserString s_database("DATABASE", true, true);
	ParserString s_dot(".");
	ParserString s_lparen("(");
	ParserString s_rparen(")");
	ParserString s_if("IF", true, true);
	ParserString s_not("NOT", true, true);
	ParserString s_exists("EXISTS", true, true);
	ParserString s_as("AS", true, true);
	ParserString s_select("SELECT", true, true);
	ParserString s_view("VIEW", true, true);
	ParserString s_materialized("MATERIALIZED", true, true);
	ParserEngine engine_p;
	ParserIdentifier name_p;
	ParserNameTypePairList columns_p;

	ASTPtr database;
	ASTPtr table;
	ASTPtr columns;
	ASTPtr storage;
	ASTPtr inner_storage;
	ASTPtr as_database;
	ASTPtr as_table;
	ASTPtr select;
	bool attach = false;
	bool if_not_exists = false;
	bool materialized = false;
	bool view = false;

	ws.ignore(pos, end);

	if (!s_create.ignore(pos, end, expected))
	{
		if (s_attach.ignore(pos, end, expected))
			attach = true;
		else
			return false;
	}

	ws.ignore(pos, end);

	if (s_database.ignore(pos, end, expected))
	{
		ws.ignore(pos, end);

		if (s_if.ignore(pos, end, expected)
			&& ws.ignore(pos, end)
			&& s_not.ignore(pos, end, expected)
			&& ws.ignore(pos, end)
			&& s_exists.ignore(pos, end, expected)
			&& ws.ignore(pos, end))
			if_not_exists = true;

		if (!name_p.parse(pos, end, database, expected))
			return false;
	}
	else if (s_table.ignore(pos, end, expected))
	{
		ws.ignore(pos, end);

		if (s_if.ignore(pos, end, expected)
			&& ws.ignore(pos, end)
			&& s_not.ignore(pos, end, expected)
			&& ws.ignore(pos, end)
			&& s_exists.ignore(pos, end, expected)
			&& ws.ignore(pos, end))
			if_not_exists = true;

		if (!name_p.parse(pos, end, table, expected))
			return false;

		ws.ignore(pos, end);

		if (s_dot.ignore(pos, end, expected))
		{
			database = table;
			if (!name_p.parse(pos, end, table, expected))
				return false;

			ws.ignore(pos, end);
		}

		/// Список столбцов
		if (s_lparen.ignore(pos, end, expected))
		{
			ws.ignore(pos, end);

			if (!columns_p.parse(pos, end, columns, expected))
				return false;

			ws.ignore(pos, end);

			if (!s_rparen.ignore(pos, end, expected))
				return false;

			ws.ignore(pos, end);

			if (!engine_p.parse(pos, end, storage, expected))
				return false;

			/// Для engine VIEW необходимо так же считать запрос AS SELECT
			if (dynamic_cast<ASTFunction &>(*storage).name == "VIEW")
			{
				if (!s_as.ignore(pos, end, expected))
					return false;
				ws.ignore(pos, end);
				Pos before_select = pos;
				if (!s_select.ignore(pos, end, expected))
					return false;
				pos = before_select;
				ParserSelectQuery select_p;
				select_p.parse(pos, end, select, expected);
			}
		}
		else
		{
			engine_p.parse(pos, end, storage, expected);

			if (!s_as.ignore(pos, end, expected))
				return false;

			ws.ignore(pos, end);

			/// AS SELECT ...
			Pos before_select = pos;
			if (s_select.ignore(pos, end, expected))
			{
				pos = before_select;
				ParserSelectQuery select_p;
				select_p.parse(pos, end, select, expected);
			}
			else
			{
				/// AS [db.]table
				if (!name_p.parse(pos, end, as_table, expected))
					return false;

				ws.ignore(pos, end);

				if (s_dot.ignore(pos, end, expected))
				{
					as_database = as_table;
					if (!name_p.parse(pos, end, as_table, expected))
						return false;

					ws.ignore(pos, end);
				}

				ws.ignore(pos, end);

				/// Опционально - может быть указана ENGINE.
				engine_p.parse(pos, end, storage, expected);
			}
		}
	} else {
		/// VIEW or MATERIALIZED VIEW
		Pos before = pos;
		if (s_materialized.ignore(pos, end, expected))
		{
			materialized = true;
			pos = before;
			ParserIdentifierWithOptionalParameters storage_p;
			storage_p.parse(pos, end, storage, expected);
			ws.ignore(pos, end, expected);
			if (!s_view.ignore(pos, end, expected))
				return false;
		} else {
			if (!s_view.ignore(pos, end, expected))
				return false;
			view = true;
			pos = before;
			ParserIdentifierWithOptionalParameters storage_p;
			storage_p.parse(pos, end, storage, expected);
		}
		ws.ignore(pos, end);

		if (!name_p.parse(pos, end, table, expected))
			return false;

		ws.ignore(pos, end);

		if (s_dot.ignore(pos, end, expected))
		{
			database = table;
			if (!name_p.parse(pos, end, table, expected))
				return false;

			ws.ignore(pos, end);
		}

		/// Опционально - может быть указана внутренняя ENGINE для MATERIALIZED VIEW
		engine_p.parse(pos, end, inner_storage, expected);

		/// AS SELECT ...
		if (!s_as.ignore(pos, end, expected))
			return false;
		ws.ignore(pos, end);
		Pos before_select = pos;
		if (!s_select.ignore(pos, end, expected))
			return false;
		pos = before_select;
		ParserSelectQuery select_p;
		select_p.parse(pos, end, select, expected);
	}

	ws.ignore(pos, end);

	ASTCreateQuery * query = new ASTCreateQuery(StringRange(begin, pos));
	node = query;

	query->attach = attach;
	query->if_not_exists = if_not_exists;
	if (database)
		query->database = dynamic_cast<ASTIdentifier &>(*database).name;
	if (table)
		query->table = dynamic_cast<ASTIdentifier &>(*table).name;
	if (inner_storage)
		query->inner_storage = inner_storage;

	query->columns = columns;
	query->storage = storage;
	if (as_database)
		query->as_database = dynamic_cast<ASTIdentifier &>(*as_database).name;
	if (as_table)
		query->as_table = dynamic_cast<ASTIdentifier &>(*as_table).name;
	query->select = select;

	if (columns)
		query->children.push_back(columns);
	if (storage)
		query->children.push_back(storage);
	if (select)
		query->children.push_back(select);

	return true;
}


}
