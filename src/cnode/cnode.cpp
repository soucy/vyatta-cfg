/*
 * Copyright (C) 2010 Vyatta, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

#include <cli_cstore.h>
#include <cnode/cnode.hpp>

using namespace std;
using namespace cnode;

////// constructors/destructors
CfgNode::CfgNode(Cstore& cstore, vector<string>& path_comps,
                 bool active, bool recursive)
  : _is_tag(false), _is_leaf(false), _is_multi(false), _is_value(false),
    _is_default(false), _is_deactivated(false), _is_leaf_typeless(false), 
    _is_invalid(false), _is_empty(false)
{
  /* first get the def (only if path is not empty). if path is empty, i.e.,
   * "root", treat it as an intermediate node.
   */
  if (path_comps.size() > 0) {
    vtw_def def;
    if (cstore.validateTmplPath(path_comps, false, def)) {
      // got the def
      _is_value = def.is_value;
      _is_tag = (def.tag && !_is_value);
      _is_leaf = (!_is_tag && !_is_value && def.def_type != ERROR_TYPE);
      _is_multi = (_is_leaf && def.multi);
      _is_default = cstore.cfgPathDefault(path_comps, active);
      _is_deactivated = cstore.cfgPathDeactivated(path_comps, active);
      cstore.cfgPathGetComment(path_comps, _comment, active);
      // ignore return

      if (_is_leaf && _is_value) {
        /* recursion should never reach here. if path is specified by user,
         * nothing further to do.
         */
        return;
      }
    } else {
      // not a valid node
      _is_invalid = true;
      return;
    }
  }

  // handle leaf node (note path_comps must be non-empty if this is leaf)
  if (_is_leaf) {
    _name = path_comps[path_comps.size() - 1];
    if (_is_multi) {
      // multi-value node
      cstore.cfgPathGetValuesDA(path_comps, _values, active, true);
      // ignore return value
    } else {
      // single-value node
      cstore.cfgPathGetValueDA(path_comps, _value, active, true);
      // ignore return value
    }
    return;
  }

  // handle intermediate (typeless) or tag
  if (_is_value) {
    // tag value
    _name = path_comps[path_comps.size() - 2];
    _value = path_comps[path_comps.size() - 1];
  } else {
    // tag node or typeless node
    _name = (path_comps.size() > 0 ? path_comps[path_comps.size() - 1] : "");
  }

  // check child nodes
  vector<string> cnodes;
  cstore.cfgPathGetChildNodesDA(path_comps, cnodes, active, true);
  if (cnodes.size() == 0) {
    // empty subtree. done.
    vector<string> tcnodes;
    cstore.tmplGetChildNodes(path_comps, tcnodes);
    if (tcnodes.size() == 0) {
      // typeless leaf node
      _is_leaf_typeless = true;
    }
    _is_empty = true;
    return;
  }

  if (!recursive) {
    // nothing further to do
    return;
  }

  // recurse
  for (size_t i = 0; i < cnodes.size(); i++) {
    path_comps.push_back(cnodes[i]);
    CfgNode *cn = new CfgNode(cstore, path_comps, active, recursive);
    if (_is_tag && !_is_value) {
      // tag node
      _tag_values.push_back(cn);
    } else {
      // intermediate node or tag value
      _child_nodes.push_back(cn);
    }
    path_comps.pop_back();
  }
}
