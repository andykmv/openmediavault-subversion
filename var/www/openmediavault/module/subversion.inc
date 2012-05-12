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
require_once("openmediavault/module.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/monit.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/util.inc");
require_once("openmediavault/initscript.inc");

class SubversionModule extends OMVModuleAbstract implements OMVINotifyListener, OMVIModuleStatus {

	/**
	 * Get the module name.
	 */
	public static function getName()
	{
		return "subversion";
	}

	/**
	 * Get the module status.
	 */
	public function getStatus()
	{

		global $xmlConfig;

		// Get the configuration object
		$xpath  = "//services/subversion";
		$object = $xmlConfig->get($xpath);
		if (is_null($object))
		{
			throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
		}

		// This is the best I could do
		$cmd = sprintf("sudo /bin/sh -c '[ -f /etc/apache2/openmediavault-webgui.d/subversion.conf ] && echo ok'");
		OMVUtil::exec($cmd, $output, $result);
		return array(
			"name"    => $this->getName(),
			"title"   => gettext("Subversion Web Service"),
			"enabled" => boolval($object['enable']),
			"running" => ! empty($output[0])
		);
	}

	/**
	 * Update the service.
	 */
	public function updateService($newconf, $oldconf)
	{

		// Create apache subversion file
		$cmd = "sudo omv-mkconf subversion 2>&1";
		OMVUtil::exec($cmd, $output, $result);
		if ($result !== 0)
		{
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
		}

		// Create privileges file
		$this->updatePrivileges();

		// Restart apache service?
		$changed                 = false;
		$oldconf['enable']       = array_boolval($oldconf, 'enable');
		$oldconf['require-auth'] = array_boolval($oldconf, 'require-auth');
		foreach (array('enable', 'realm', 'require-auth', 'mntentref') as $k)
		{
			if (empty($newconf[$k]))
			{
				$newconf[$k] = '';
			}
			if (empty($oldconf[$k]))
			{
				$oldconf[$k] = '';
			}
			if ((string) $newconf[$k] != (string) $oldconf[$k])
			{
				$changed = true;
				break;
			}
		}
		if ($changed)
		{
			$monit = new OMVMonit();
			$monit->restart("apache2", TRUE);
		}
	}

	/**
	 * Update authz file
	 */
	public function updatePrivileges()
	{

		// Create authz file
		$cmd = "sudo omv-mkconf subversion-privileges 2>&1";
		OMVUtil::exec($cmd, $output, $result);
		if ($result !== 0)
		{
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
		}
	}

	/**
	 * Bind listeners.
	 */
	function bindListeners(OMVNotifyDispatcher $dispatcher)
	{
		/* Update service if configuration has been modified */
		$dispatcher->addListener(OMV_NOTIFY_MODIFY, "org.openmediavault.services.subversion", array($this, "updateService"), 1);

		/* Repo events */
		$dispatcher->addListener(OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY, "org.openmediavault.services.subversion.repos.repo", array($this, "updatePrivileges"), 1);

		/* Listen for user events */
		$dispatcher->addListener(OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY, "org.openmediavault.system.usermanagement.users.user", array($this, "updatePrivileges"));

		/* Listen for group events */
		$dispatcher->addListener(OMV_NOTIFY_DELETE, "org.openmediavault.system.usermanagement.groups.group", array($this, "updatePrivileges"));
	}

}

// Register module
$moduleMgr = &OMVModuleMgr::getInstance();
$moduleMgr->registerModule(new SubversionModule());

