/**
 * vim: tabstop=4
 *
 * @license    http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author     Ian Moore <imooreyahoo@gmail.com>
 * @author     Marcel Beck <marcel.beck@mbeck.org>
 * @copyright  Copyright (c) 2011-2012 Ian Moore
 * @copyright  Copyright (c) 2012 Marcel Beck
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file. If not, see <http://www.gnu.org/licenses/>.
 *
 */
// require("js/omv/NavigationPanel.js")
// require("js/omv/data/DataProxy.js")
// require("js/omv/FormPanelExt.js")
// require("js/omv/grid/TBarGridPanel.js")
// require("js/omv/form/PasswordField.js")
// require("js/omv/form/SharedFolderComboBox.js")
// require("js/omv/form/plugins/FieldInfo.js")
// require("js/omv/CfgObjectDialog.js")

Ext.ns("OMV.Module.Services");

// Register the menu.
OMV.NavigationPanelMgr.registerMenu("services", "subversion", {
	text:_("Subversion"),
	icon:"images/subversion.png"
});

OMV.Module.Services.SVNSettingsPanel = function (config) {
	var initialConfig = {
		rpcService:"Subversion"
	};
	Ext.apply(initialConfig, config);
	OMV.Module.Services.SVNSettingsPanel.superclass.constructor.call(this, initialConfig);
};

Ext.extend(OMV.Module.Services.SVNSettingsPanel, OMV.FormPanelExt, {

	initComponent:function () {
		OMV.Module.Services.SVNSettingsPanel.superclass.initComponent.apply(this, arguments);
		this.on("load", this._updateFormFields, this);

		this.on("load", function (cfgPanel, data) {

			// Disable repos tab?
			var checked = this.findFormField("enable").checked;
			var rp = this.ownerCt.find('title', _('Repositories'));
			if (rp.length > 0)
				(checked ? rp[0].enable() : rp[0].disable());

			Ext.getCmp('OMV.Module.Services.SVNSettingsPanel-repo-root').update(data['repository-root']);

		}, this);
	},

	getFormItems:function () {
		return [
			{
				xtype   :"fieldset",
				title   :_("General settings"),
				defaults:{
					labelSeparator:""
				},
				items   :[
					{
						xtype     :"checkbox",
						name      :"enable",
						fieldLabel:_("Enable"),
						checked   :false,
						inputValue:1,
						listeners :{
							check:this._updateFormFields,
							scope:this
						}
					},
					{
						xtype     :"textfield",
						name      :"realm",
						fieldLabel:_("Realm Name"),
						allowBlank:false,
						width     :300,
						plugins   :[ OMV.form.plugins.FieldInfo ],
						infoText  :_("Authentication realm."),
						value     :_("Subversion Repository on Open Media Vault")
					},
					{
						xtype     :"checkbox",
						name      :"require-auth",
						fieldLabel:_("Require authentication"),
						checked   :true,
						inputValue:1,
						plugins   :[ OMV.form.plugins.FieldInfo ],
						infoText  :_("Uncheck to allow anonymous access to one or more repositories.<br />Permissions can be further restricted per repository.")
					},
					{
						xtype        :"combo",
						name         :"mntentref",
						hiddenName   :"mntentref",
						fieldLabel   :_("Repository Volume"),
						emptyText    :_("Select a volume ..."),
						allowBlank   :false,
						allowNone    :false,
						width        :400,
						editable     :false,
						triggerAction:"all",
						displayField :"description",
						valueField   :"uuid",
						store        :new OMV.data.Store({
							remoteSort:false,
							proxy     :new OMV.data.DataProxy({"service":"ShareMgmt", "method":"getCandidates"}),
							reader    :new Ext.data.JsonReader({
								idProperty:"uuid",
								fields    :[
									{ name:"uuid" },
									{ name:"description" }
								]
							})
						})
					},
					{
						xtype     :"label",
						name      :"repository-root",
						id        :'OMV.Module.Services.SVNSettingsPanel-repo-root',
						fieldLabel:_("Repository Root"),
						allowNone :true,
						readOnly  :true,
						hiddenName:"repository-root",
						width     :400
					}
				]
			}
		];
	},

	/**
	 * Private function to update the states of various form fields.
	 */
	_updateFormFields:function () {
		// Enabled / disabled fields
		var field = this.findFormField("enable");
		var checked = field.checked;
		var fields = [ "realm", "require-auth", "mntentref"];
		for (var i = 0; i < fields.length; i++) {
			field = this.findFormField(fields[i]);
			if (!Ext.isEmpty(field)) {
				field.allowBlank = !checked;
				field.setReadOnly(!checked);
			}
		}
	}
});

OMV.Module.Services.SVNReposGridPanel = function (config) {

	var initialConfig = {
		disabled         :true,
		hidePagingToolbar:false,
		colModel         :new Ext.grid.ColumnModel({
			columns:[
				{
					header   :_("Name"),
					sortable :true,
					dataIndex:"name",
					id       :"name"
				},
				{
					header   :_("Default privilege level"),
					sortable :true,
					dataIndex:"default-access",
					id       :"default-access",
					renderer :this.prvRenderer
				},
				{
					header   :_("Description"),
					sortable :true,
					dataIndex:"comment",
					id       :"comment"
				},
				{
					header   :_("URL"),
					sortable :true,
					dataIndex:"name",
					id       :"url",
					renderer :this.urlRenderer
				},
				{
					header   :_("Path"),
					sortable :true,
					dataIndex:"path",
					id       :"path"
				}
			]
		})
	};
	Ext.apply(initialConfig, config);
	OMV.Module.Services.SVNReposGridPanel.superclass.constructor.call(this, initialConfig);
};

Ext.extend(OMV.Module.Services.SVNReposGridPanel, OMV.grid.TBarGridPanel, {

	initComponent:function () {
		this.store = new OMV.data.Store({
			autoLoad  :true,
			remoteSort:false,
			proxy     :new OMV.data.DataProxy({"service":"subversion", "method":"getRepos"}),
			reader    :new Ext.data.JsonReader({
				idProperty   :"uuid",
				totalProperty:"total",
				root         :"data",
				fields       :[
					{ name:"uuid" },
					{ name:"path" },
					{ name:"default-access" },
					{ name:"comment" },
					{ name:"name" }
				]
			})
		});
		OMV.Module.Services.SVNReposGridPanel.superclass.initComponent.apply(this, arguments);
	},

	initToolbar:function () {
		var tbar = OMV.Module.Services.SVNReposGridPanel.superclass.initToolbar.apply(this);
		tbar.insert(2, {
			id      :this.getId() + "-privileges",
			xtype   :"button",
			text    :_("Privileges"),
			icon    :"images/privileges.gif",
			handler :this.cbPrivilegesBtnHdl.createDelegate(this),
			disabled:true

		});
		return tbar;
	},

	urlRenderer:function (val, cell, record, row, col, store) {
		var u = location.host + location.pathname + '/svn/' + val;
		u = location.protocol + '//' + u.replace('//', '/');
		return '<a href="' + u + '" target="_blank">' + u + '</a>';
	},

	prvRenderer:function (val, cell, record, row, col, store) {
		switch (val) {
			case "read-only":
				val = _("Read-only");
				break;
			case "write":
				val = _("Read / Write");
				break;
			default:
				val = _("No access");
		}
		return val;
	},

	cbAddBtnHdl:function () {
		var wnd = new OMV.Module.Services.SVNRepoPropertyDialog({
			uuid     :OMV.UUID_UNDEFINED,
			listeners:{
				submit:function () {
					this.doReload();
				},
				scope :this
			}
		});
		wnd.show();
	},

	cbEditBtnHdl:function () {
		var selModel = this.getSelectionModel();
		var record = selModel.getSelected();
		var wnd = new OMV.Module.Services.SVNRepoPropertyDialog({
			uuid     :record.get("uuid"),
			listeners:{
				submit:function () {
					this.doReload();
				},
				scope :this
			}
		});
		wnd.show();
	},

	cbPrivilegesBtnHdl:function () {
		var selModel = this.getSelectionModel();
		var record = selModel.getSelected();
		var wnd = new OMV.Module.Services.SVNPrivilegesPropertyDialog({
			uuid     :record.get("uuid"),
			listeners:{
				submit:function () {
					this.doReload();
				},
				scope :this
			}
		});
		wnd.show();
	},

	cbSelectionChangeHdl:function (model) {
		OMV.Module.Services.SVNReposGridPanel.superclass.
						cbSelectionChangeHdl.apply(this, arguments);
		// Process additional buttons
		var records = model.getSelections();
		var tbarPrivilegesCtrl = this.getTopToolbar().findById(
						this.getId() + "-privileges");
		if (records.length <= 0) {
			tbarPrivilegesCtrl.disable();
		} else if (records.length == 1) {
			tbarPrivilegesCtrl.enable();
		} else {
			tbarPrivilegesCtrl.disable();
		}
	},

	doDeletion:function (record) {
		OMV.Ajax.request(this.cbDeletionHdl, this, "subversion", "removeRepo", {uuid:record.get("uuid") });
	}


});

OMV.NavigationPanelMgr.registerPanel("services", "subversion", {
	cls     :OMV.Module.Services.SVNSettingsPanel,
	position:10,
	title   :_("Settings")
});

OMV.NavigationPanelMgr.registerPanel("services", "subversion", {
	cls     :OMV.Module.Services.SVNReposGridPanel,
	position:20,
	title   :_("Repositories")
});

OMV.Module.Services.SVNRepoPropertyDialog = function (config) {
	var initialConfig = {
		rpcService  :"subversion",
		rpcGetMethod:"getRepo",
		rpcSetMethod:"setRepo",
		title       :((config.uuid == OMV.UUID_UNDEFINED) ? _("Add repository") : _("Edit repository")),
		autoHeight  :true
	};
	Ext.apply(initialConfig, config);
	OMV.Module.Services.SVNRepoPropertyDialog.superclass.constructor.call(this, initialConfig);
};

Ext.extend(OMV.Module.Services.SVNRepoPropertyDialog, OMV.CfgObjectDialog, {
	initComponent:function () {
		OMV.Module.Services.SVNRepoPropertyDialog.superclass.initComponent.apply(this, arguments);
		this.on("load", this._updateFormFields, this);
	},

	getFormConfig:function () {
		return {
			autoHeight:true
		};
	},

	getFormItems:function () {
		return [
			{
				xtype     :"textfield",
				name      :"name",
				fieldLabel:_("Name"),
				plugins   :[ OMV.form.plugins.FieldInfo ],
				infoText  :_("Repository name."),
				allowBlank:false
			},
			{
				xtype     :"textfield",
				name      :"comment",
				fieldLabel:_("Description"),
				allowBlank:false
			},
			{
				xtype        :"combo",
				name         :"default-access",
				fieldLabel   :_("Default privileges"),
				mode         :"local",
				store        :new Ext.data.SimpleStore({
					fields:[ "value", "text" ],
					data  :[
						[ "none", _("No access") ],
						[ "read-only", _("Read-only") ],
						[ "write", _("Read / Write") ]
					]
				}),
				displayField :"text",
				valueField   :"value",
				allowBlank   :false,
				editable     :false,
				triggerAction:"all",
				value        :"none"
			}
		];
	},

	/**
	 * Private function to update the states of various form fields.
	 */
	_updateFormFields:function () {
		var field = this.findFormField("name");
		if ((this.uuid !== OMV.UUID_UNDEFINED) && Ext.isDefined(field)) {
			field.setReadOnly(true);
		}
	}
});

/**
 * @class OMV.Module.Services.SVNPrivilegesPropertyDialog
 * @config uuid The UUID of the shared folder to process.
 * @config readOnly TRUE to set the dialog to read-only.
 * Defaults to FALSE.
 */
OMV.Module.Services.SVNPrivilegesPropertyDialog = function (config) {
	var initialConfig = {
		title      :_("Edit repository privileges"),
		width      :500,
		height     :300,
		layout     :"fit",
		modal      :true,
		border     :false,
		buttonAlign:"center"
	};
	Ext.apply(initialConfig, config);
	OMV.Module.Services.SVNPrivilegesPropertyDialog.superclass.constructor.call(this, initialConfig);
	this.addEvents(
					/**
					 * Fires after the submission has been finished successful.
					 */
					"submit"
	);
};

Ext.extend(OMV.Module.Services.SVNPrivilegesPropertyDialog, Ext.Window, {
	initComponent:function () {
		this.grid = new OMV.grid.GridPanel({
			bodyCssClass:"x-grid3-without-dirty-cell",
			stateId     :"474eacf4-cadb-4ae4-b545-4f7f47d7aed9",
			colModel    :new Ext.grid.ColumnModel({
				columns:[
					{
						header   :_("Type"),
						sortable :true,
						dataIndex:"type",
						id       :"type",
						align    :"center",
						width    :50,
						renderer :this.typeRenderer,
						scope    :this
					},
					{
						header   :_("Name"),
						sortable :true,
						dataIndex:"name",
						id       :"name"
					},
					{
						header   :_("Read / Write"),
						dataIndex:"writeable",
						id       :"writeable",
						align    :"center",
						renderer :this.checkBoxRenderer,
						scope    :this
					},
					{
						header   :_("Read-only"),
						dataIndex:"readonly",
						id       :"readonly",
						align    :"center",
						renderer :this.checkBoxRenderer,
						scope    :this
					},
					{
						header   :_("No access"),
						dataIndex:"deny",
						id       :"deny",
						align    :"center",
						renderer :this.checkBoxRenderer,
						scope    :this
					}
				]
			}),
			store       :new OMV.data.Store({
				autoLoad  :true,
				remoteSort:false,
				proxy     :new OMV.data.DataProxy({"service":"Subversion", "method":"getPrivileges", "extraParams":{ "uuid":this.uuid }}),
				reader    :new Ext.data.JsonReader({
					idProperty:"uuid",
					fields    :[
						{ name:"uuid" },
						{ name:"type" },
						{ name:"name" },
						{ name:"perms" }
					]
				}),
				listeners :{
					"load":function (store, records, options) {
						records.each(function (record) {
							// Set default values
							record.data.deny = false;
							record.data.readonly = false;
							record.data.writeable = false;
							// Update values depending on the permissions
							switch (record.get("perms")) {
								case 0:
									record.data.deny = true;
									break;
								case 5:
									record.data.readonly = true;
									break;
								case 7:
									record.data.writeable = true;
									break;
							}
							record.commit();
						}, this);
					},
					scope :this
				}
			})
		});
		Ext.apply(this, {
			buttons:[
				{
					text   :_("OK"),
					handler:this.cbOkBtnHdl,
					scope  :this
				},
				{
					text   :_("Cancel"),
					handler:this.cbCancelBtnHdl,
					scope  :this
				}
			],
			items  :[ this.grid ]
		});
		OMV.Module.Services.SVNPrivilegesPropertyDialog.superclass.
						initComponent.apply(this, arguments);
		// Register event handler
		this.grid.on("cellclick", this.onCellClick, this);
	},

	/**
	 * @method cbOkBtnHdl
	 * Method that is called when the 'OK' button is pressed.
	 */
	cbOkBtnHdl:function () {
		// Quit immediatelly if the permissions have not been modified.
		if (!this.grid.isDirty()) {
			this.close();
			return;
		}
		this.doSubmit();
	},

	/**
	 * @method cbCancelBtnHdl
	 * Method that is called when the 'Cancel' button is pressed.
	 */
	cbCancelBtnHdl:function () {
		this.close();
	},

	/**
	 * @method doLoad
	 * Load the grid content.
	 */
	doLoad:function () {
		this.grid.store.load();
	},

	doSubmit:function () {
		// Display waiting dialog
		OMV.MessageBox.wait(null, _("Saving ..."));
		// Prepare RPC content
		var records = this.grid.store.getRange();
		var values = {
			uuid      :this.uuid,
			privileges:[]
		};
		for (var i = 0; i < records.length; i++) {
			var record = records[i];
			if ((true === record.get("deny")) ||
							(true === record.get("readonly")) ||
							(true === record.get("writeable"))) {
				var perms = 0;
				if (true === record.get("readonly"))
					perms = 5;
				else if (true === record.get("writeable"))
					perms = 7;
				values.privileges.push({
					type :record.get("type"),
					uuid :record.get("uuid"),
					perms:perms
				});
			}
		}
		OMV.Ajax.request(this.cbSubmitHdl, this, "Subversion", "setPrivileges", values);
	},

	cbSubmitHdl:function (id, response, error) {
		OMV.MessageBox.updateProgress(1);
		OMV.MessageBox.hide();
		if (error === null) {
			this.fireEvent("submit", this);
			this.close();
		} else {
			OMV.MessageBox.error(null, error);
		}
	},

	/**
	 * Handle grid cell clicks. Only process columns with a checkbox.
	 */
	onCellClick:function (grid, rowIndex, columnIndex, e) {
		var record = this.grid.store.getAt(rowIndex);
		var dataIndex = this.grid.getColumnModel().getDataIndex(columnIndex);
		var dataIndices = [ "readonly", "writeable", "deny" ];
		if (-1 !== dataIndices.indexOf(dataIndex)) {
			// Clear all selections
			for (var i = 0; i < dataIndices.length; i++) {
				// Skip current clicked record field, otherwise unselection
				// of cells will not work
				if (dataIndices[i] === dataIndex)
					continue;
				// Set to 'false' per default
				record.set(dataIndices[i], false);
			}
			// Set new selection
			record.set(dataIndex, !record.get(dataIndex));
		}
	},

	/**
	 * Render a user/group icon in the given grid cell.
	 */
	typeRenderer:function (val, cell, record, row, col, store) {
		switch (val) {
			case "user":
				val = "<img border='0' src='images/user.png'>";
				break;
			case "group":
				val = "<img border='0' src='images/group.png'>";
				break;
		}
		return val;
	},

	/**
	 * Render a checkbox in the given grid cell.
	 */
	checkBoxRenderer:function (val, cell, record, row, col, store) {
		cell.css += " x-grid3-check-col-td";
		return '<div class="x-grid3-check-col' + ((true === val) ? '-on' : '') +
						' x-grid3-cc-' + this.id + '">&#160;</div>';
	}
});

