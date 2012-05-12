<?php
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

require_once("openmediavault/object.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/util.inc");
require_once("openmediavault/rpc.inc");
require_once("openmediavault/notify.inc");

class SubversionRpc extends OMVRpc {

	const xpathRoot = '//services/subversion';

	public function __construct()
	{

		$this->methodSchemata = array(
			"set"           => array(
				'{
					"type":"object",
					"properties":{
						"enable":{"type":"boolean"},
						"require-auth":{"type":"boolean"},
						"realm":{"type":"string"},
						"repository-root":{"type":"string","optional":true},
						"mntentref":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '}
					}
				}'
			),
			"getRepos"      => array(
				'{
					"type":"object",
					"properties":{
						"start":{"type":"integer"},
						"limit":{' . $GLOBALS['OMV_JSONSCHEMA_COUNTFIELD'] . '},
						"sortfield":{' . $GLOBALS['OMV_JSONSCHEMA_SORTFIELD'] . '},
						"sortdir":{' . $GLOBALS['OMV_JSONSCHEMA_SORTDIR'] . '}
					}
				}'
			),
			"setRepo"       => array(
				'{
					"type":"object",
					"properties":{
						"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID_UNDEFINED'] . '},
						"comment":{"type":"string"},
						"default-access":{"type":"string","enum":["none","read-only","write"]},
						"name":{
							"type":"string",
							"format":"regex",
							"pattern":"/^[a-z0-9_\\\\-][a-z0-9_\\\\-\\\\.]+$/i"
						}
					}
				}'
			),
			"getRepo"       => array(
				'{
					"type":"object",
					"properties":{
						"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '}
					}
				}'
			),
			"removeRepo"    => array(
				'{
					"type":"object",
					"properties":{
						"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '}
					}
				}'
			),
			"getPrivileges" => array(
				'{
					"type":"object",
					"properties":{
						"start":{"type":"integer"},
						"limit":{' . $GLOBALS['OMV_JSONSCHEMA_COUNTFIELD'] . '},
						"sortfield":{' . $GLOBALS['OMV_JSONSCHEMA_SORTFIELD'] . '},
						"sortdir":{' . $GLOBALS['OMV_JSONSCHEMA_SORTDIR'] . '},
						"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '}
					}
				}'
			),
			"setPrivileges" => array(
				'{
					"type":"object",
					"properties":{
						"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '},
						"privileges":{
							"type":"array",
							"items":{
								"type":"object",
								"properties":{
									"uuid":{' . $GLOBALS['OMV_JSONSCHEMA_UUID'] . '},
									"perms":{"type":"integer","enum":[0,5,7]},
										"type":{"type":"string","enum":["user","group"]}
								}
							}
						}
					}
				}'
			)
		);
	}

	/**
	 * Safe config getting
	 */
	public function __call($name, $args)
	{
		// Configuration methods
		if (substr($name, 0, 6) == 'config')
		{

			// Correct method name
			$name    = substr($name, 6);
			$name[0] = strtolower($name[0]);

			global $xmlConfig;
			$object = call_user_func_array(array($xmlConfig, $name), $args);
			switch ($name)
			{
				case 'delete':
					if ($object === false)
					{
						throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND, $args[0]);
					}
					break;
				case 'save':
					if ($object === false)
					{
						throw new OMVException(OMVErrorMsg::E_CONFIG_SAVE_FAILED, $xmlConfig->getError());
					}
					break;
				case 'set':
				case 'replace':
					if ($object === false)
					{
						throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
					}
					break;
				default:
					if (is_null($object))
					{
						throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $args[0]);
					}
			}

			return $object;

		}
		throw new Exception("Method " . __CLASS__ . "::" . $name . " does not exist.");
	}

	/**
	 * Verify that the current user is an admin, and validate method args
	 */
	function _validate($mname = '', $args = array())
	{

		// Check permissions
		$this->validateSession();
		if (! $this->hasRole(OMV_ROLE_ADMINISTRATOR))
		{
			throw new OMVException(OMVErrorMsg::E_RPC_SERVICE_INVALID_PERMISSION);
		}
		$this->commitSession();

		// Check incoming data
		if ($mname)
		{
			$this->validateParams($mname, $args);
		}
	}

	/**
	 * Get all configuration data for service.
	 *
	 * @return array configuration data
	 */
	function get()
	{

		// Validation
		$this->_validate();

		//Get configuration object
		$object = $this->configGet(self::xpathRoot);

		// Modify result data
		$object['enable']       = boolval($object['enable']);
		$object['require-auth'] = boolval($object['require-auth']);
		return $object;
	}

	/**
	 * Set configuration data for service.
	 *
	 * @param $data array configuration data
	 */
	function set($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// Parent dir
		$pdir = sprintf("//system/fstab/mntent[uuid='%s']", $data['mntentref']);
		$pdir = $this->configGet($pdir);
		$pdir = $pdir['dir'];

		$data['repository-root'] = "{$pdir}/subversion-repos";

		// Prepare configuration data
		$econf = $this->get();

		// Don't change repo root if we have existing repos
		if (is_array($econf['repos']) && count($econf['repos']) && $econf['repository-root'] != $data['repository-root'])
		{
			throw new Exception("Refusing to chnage repository root while repositories exist.");
		}

		// Create repo root
		if ($econf['repository-root'] != $data['repository-root'])
		{

			// Ultimately files / folders must be readable / writable by apache.
			$cmd = "sudo /bin/sh -c '[ -d {$data['repository-root']} ] || /bin/mkdir -p {$data['repository-root']}; chown www-data {$data['repository-root']}; chmod u+rwx {$data['repository-root']}'";
			OMVUtil::exec($cmd, $output, $result);
			if ($result !== 0)
			{
				throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
			}

		}

		$object = array(
			"enable"          => array_boolval($data, 'enable'),
			"realm"           => $data['realm'],
			"require-auth"    => array_boolval($data, 'require-auth'),
			"repository-root" => $data['repository-root'],
			"mntentref"       => $data['mntentref'],
			"repos"           => (isset($econf['repos']) ? $econf['repos'] : array())
		);

		// Set configuration object
		$this->configReplace(self::xpathRoot, $object);

		$this->configSave();

		// Notify general configuration changes
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_MODIFY,
												"org.openmediavault.services.subversion", $object, $econf);

	}

	/**
	 * Remove repo from config
	 *
	 * @param string $uuid configuration uuid of repository
	 */
	public function removeRepo($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// Get configuration object for notification
		$object = $this->configGet(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']");

		// Delete configuration object
		$this->configDelete(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']");

		// Save configuration
		$this->configSave();

		// Remove the folder
		$repoFolder = $this->configGet(self::xpathRoot . "/repository-root") . "/" . $object['name'];

		// These must match!!!
		if (realpath($repoFolder) == $repoFolder)
		{
			$cmd = "sudo su - www-data -c 'rm -rf {$repoFolder} 2>&1'";
			OMVUtil::exec($cmd, $output, $res);
			if ($res !== 0)
			{
				throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
			}
		}

		// Notify configuration changes
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_DELETE, "org.openmediavault.services.subversion.repos.repo", $object);
	}

	/**
	 * Get list of repositories
	 *
	 * @param $start     integer start point in paging list
	 * @param $count     integer number of objects to return in paged list
	 * @param $sortField string field to sort on
	 * @param $sortDir   integer sort direction
	 *
	 * @return array list of repositories
	 */
	public function getRepos($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		$objects = $this->configGetList(self::xpathRoot . "/repos/repo");
		$svnroot = $this->configGet(self::xpathRoot . "/repository-root") . "/";

		// Add path info
		foreach ($objects as $objectk => &$objectv)
		{
			$objectv['path'] = $svnroot . $objectv['name'];
		}

		// Filter result
		return $this->applyFilter($objects, $data['start'], $data['limit'], $data['sortfield'], $data['sortdir']);

	}

	/**
	 * Save repository configuration data
	 *
	 * @param $data array repository configuration
	 */
	public function setRepo($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// Prepare configuration data
		$object = array(
			"uuid"           => ($data['uuid'] == $GLOBALS['OMV_UUID_UNDEFINED']) ? OMVUtil::uuid() : $data['uuid'],
			"name"           => trim($data['name']),
			"default-access" => @$data['default-access'],
			"comment"        => $data['comment']
		);

		// Set configuration data
		if ($data['uuid'] == $GLOBALS['OMV_UUID_UNDEFINED'])
		{

			// Check uniqueness
			if (TRUE === $this->configExists(self::xpathRoot . "/repos/repo[name='{$object['name']}']"))
			{
				throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS);
			}

			// Initialize repo
			$path = $this->configGet(self::xpathRoot . "/repository-root") . "/{$object['name']}";

			// If this isn't an existing repository, create it
			$cmd = "sudo su - www-data -c 'svnadmin verify {$path} 2>&1'";
			OMVUtil::exec($cmd, $output, $res);
			if ($res !== 0)
			{

				$cmd = "sudo su - www-data -c 'svnadmin create {$path} 2>&1 && echo $!'";
				OMVUtil::exec($cmd, $output, $res);
				if ($res !== 0)
				{
					throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
				}

			}

			// add perms array
			$object['privileges'] = array();

			// Append object to configuration
			$this->configSet(self::xpathRoot . "/repos", array("repo" => $object));

		}
		else
		{

			// Update existing configuration object
			$this->configReplace(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']", $object);

		}

		// Save configuration
		$this->configSave();

		// Notify configuration changes
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(($data['uuid'] == $GLOBALS['OMV_UUID_UNDEFINED']) ?
																OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
												"org.openmediavault.services.subversion.repos.repo", $object);

	}

	/**
	 * Get a single repository's configuration
	 *
	 * @param $uuid string configuration uuid of repository
	 *
	 * @return array repository configuration data
	 */
	public function getRepo($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// get existing repo
		return $this->configGet(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']");

	}

	/**
	 * Get the repo privileges.
	 *
	 * @param uuid The UUID of the repository.
	 *
	 * @return An array containing the requested privileges.
	 */
	function getPrivileges($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// Get repo privileges configuration object
		$object = $this->configGet(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']");

		$result = array();

		// Get all user configuration objects
		$users = $this->configGetList("//system/usermanagement/users/user");

		foreach ($users as $userk => $userv)
		{

			//Skip various users.
			if (in_array($userv['name'], array("admin")))
			{
				continue;
			}

			$privilege = array(
				"type"  => "user",
				"name"  => $userv['name'],
				"uuid"  => $userv['uuid'],
				"perms" => NULL
			);

			// Check if there are any configured privileges for the given user
			if (! empty($object['privileges']['privilege']))
			{

				foreach ($object['privileges']['privilege'] as $objprivilegev)
				{
					if (! array_key_exists('userref', $objprivilegev) || ($userv['uuid'] !== $objprivilegev['userref']))
					{
						continue;
					}
					$privilege['perms'] = intval($objprivilegev['perms']);
				}
			}
			$result[] = $privilege;
		}

		// Get all group configuration objects
		$groups = $this->configGetList("//system/usermanagement/groups/group");

		foreach ($groups as $groupk => $groupv)
		{

			$privilege = array(
				"type"  => "group",
				"name"  => $groupv['name'],
				"uuid"  => $groupv['uuid'],
				"perms" => NULL
			);

			// Check if there are any configured privileges for the given user
			if (! empty($object['privileges']['privilege']))
			{

				foreach ($object['privileges']['privilege'] as $objprivilegev)
				{
					if (! array_key_exists('groupref', $objprivilegev) || ($groupv['uuid'] !== $objprivilegev['groupref']))
					{
						continue;
					}
					$privilege['perms'] = intval($objprivilegev['perms']);
				}
			}
			$result[] = $privilege;

		}

		return $result;
	}

	/**
	 * Set repo privileges.
	 *
	 * @param data An array containing uuid of the repo and a list of privileges to set.
	 */
	function setPrivileges($data)
	{

		// Validation
		$this->_validate(__METHOD__, func_get_args());

		// Get repository configuration object
		$object = $this->configGet(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']");

		// Reset the repository privileges.
		$object['privileges'] = array();

		// Prepare the repository privileges.
		foreach ($data['privileges'] as $datak => $datav)
		{

			if (is_null($datav['perms']))
			{
				continue;
			}

			$privilege = array("perms" => $datav['perms']);

			// Privilege for a user or a group?
			switch ($datav['type'])
			{

				case "user":

					// Check if user exists.
					$userObj = $this->configGet(sprintf("//system/usermanagement/users/user[uuid='%s']", $datav['uuid']));

					// Is user allowed? It does not make sense to give the WebGUI
					// administrator permissions for a repository.

					if (in_array($userObj['name'], array("admin")))
					{
						throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_INVALID, sprintf(gettext("The user '%s' is not allowed"), $userObj['name']));
					}
					$privilege['userref'] = $datav['uuid'];
					break;

				case "group":

					// Check if group exists.
					$this->configGet(sprintf("//system/usermanagement/groups/group[uuid='%s']", $datav['uuid']));
					$privilege['groupref'] = $datav['uuid'];
					break;
			}

			// Finally add privilege to repo privileges.
			$object['privileges']['privilege'][] = $privilege;
		}

		// Update existing configuration object.
		$this->configReplace(self::xpathRoot . "/repos/repo[uuid='{$data['uuid']}']", $object);

		// Save configuration.
		$this->configSave();

		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_MODIFY, "org.openmediavault.services.subversion.repos.repo", $object);
	}

}
