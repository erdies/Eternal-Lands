#ifdef MISSILES

#include "3d_objects.h"
#include "actor_scripts.h"
#include "asc.h"
#include "cal.h"
#include "cal3d_wrapper.h"
#include "e3d.h"
#include "gl_init.h"
#include "init.h"
#include "missiles.h"
#include "tiles.h"
#include "vmath.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define MAX_MISSILES 1024
#define EPSILON 1E-4

const float arrow_length = 0.75;
const float bolt_length = 0.4;

const float arrow_speed = 50.0;
const float arrow_trace_length = 7.0;
const float arrow_color[3] = {0.8, 0.8, 0.8};

Missile missiles_list[MAX_MISSILES];

unsigned int missiles_count = 0;

FILE *missiles_log = NULL;

void missiles_open_log()
{
	char log_name[1024];
	char starttime[200], sttime[200];
	struct tm *l_time; time_t c_time;

	safe_snprintf(log_name, 1024, "%smissiles_log.txt", configdir);

	missiles_log = fopen(log_name, "a");

	if (missiles_log == NULL)
	{
		fprintf (stderr, "Unable to open log file \"%s\"\n", log_name);
		exit (1);
	}

	time (&c_time);
	l_time = localtime (&c_time);
	strftime(sttime, sizeof(sttime), "\n\nLog started at %Y-%m-%d %H:%M:%S localtime", l_time);
	safe_snprintf(starttime, sizeof(starttime), "%s (%s)\n\n", sttime, tzname[l_time->tm_isdst>0]);
	fwrite (starttime, strlen(starttime), 1, missiles_log);
}

void missiles_log_message(const char *format, ...)
{
	va_list ap;
	struct tm *l_time; time_t c_time;
	char logmsg[512];
	char errmsg[512];

	va_start(ap, format);
	vsnprintf(errmsg, 512, format, ap);
	va_end(ap);

	if (missiles_log == NULL)
		missiles_open_log();

	time(&c_time);
	l_time = localtime(&c_time);
	strftime(logmsg, sizeof(logmsg), "[%H:%M:%S] ", l_time);
	strcat(logmsg, errmsg);

	if(format[strlen(format)-1] != '\n') {
		strcat(logmsg, "\n");
	}
	fprintf(missiles_log, logmsg);
  	fflush (missiles_log);
}

void missiles_clear()
{
	missiles_count = 0;
}

unsigned int missiles_add(MissileType type,
						  float origin[3],
						  float target[3],
						  float speed,
						  float shift,
						  int miss_target)
{
	Missile *mis;
	
	assert(missiles_count < MAX_MISSILES);

#ifdef DEBUG
	missiles_log_message("add_missile: origin=(%.2f,%.2f,%.2f), target=(%.2f,%.2f,%.2f)",
						 origin[0], origin[1], origin[2], target[0], target[1], target[2]);
#endif // DEBUG
	
	mis = &missiles_list[missiles_count++];

	mis->type = type;
	mis->miss_target = miss_target;
	memcpy(mis->position, origin, sizeof(float)*3);
	mis->direction[0] = target[0] - origin[0];
	mis->direction[1] = target[1] - origin[1];
	mis->direction[2] = target[2] - origin[2];
	mis->remaining_distance = sqrt(mis->direction[0]*mis->direction[0] +
								   mis->direction[1]*mis->direction[1] +
								   mis->direction[2]*mis->direction[2]);
	assert(fabs(mis->remaining_distance) > EPSILON);
	mis->direction[0] /= mis->remaining_distance;
	mis->direction[1] /= mis->remaining_distance;
	mis->direction[2] /= mis->remaining_distance;
	mis->speed = speed;
	mis->covered_distance = 0;
	mis->remaining_distance += shift;
	
	return missiles_count;
}

void missiles_remove(unsigned int missile_id)
{
	assert(missile_id < missiles_count);

	--missiles_count;
	if (missile_id < missiles_count) {
		memcpy(&missiles_list[missile_id],
			   &missiles_list[missiles_count],
			   sizeof(Missile));
	}
}

/* void missiles_draw_current_actor_nodes() */
/* { */
/* 	actor *a = get_actor_ptr_from_id(yourself); */
/* 	float act_rot[9]; */
/* 	float pos1[3], pos2[3], shift[3], tmp[3]; */
/* 	int displayed_bones[] = {0, 11}; */
/* 	int i; */
	
/* 	if (!a) return; */
	
/* 	get_actor_rotation_matrix(a, act_rot); */

/* 	glColor3f(1.0, 1.0, 1.0); */
/* 	glLineWidth(5.0); */
/* 	glBegin(GL_LINES); */

/*  	for (i = 0; i <= 1; ++i) */
/* 	{ */
/* 		int bone_id = displayed_bones[i]; */

/* 		cal_get_actor_bone_local_position(a, bone_id, NULL, tmp); */
/* 		transform_actor_local_position_to_absolute(a, tmp, act_rot, pos1); */

/* 		shift[0] = 0.3; shift[1] = 0.0; shift[2] = 0.0; */
/* 		cal_get_actor_bone_local_position(a, bone_id, shift, tmp); */
/* 		transform_actor_local_position_to_absolute(a, tmp, act_rot, pos2); */
/* 		glColor3f(1.0, 0.0, 0.0); */
/* 		glVertex3fv(pos1); */
/* 		glVertex3fv(pos2); */

/* 		shift[0] = 0.0; shift[1] = 0.3; shift[2] = 0.0; */
/* 		cal_get_actor_bone_local_position(a, bone_id, shift, tmp); */
/* 		transform_actor_local_position_to_absolute(a, tmp, act_rot, pos2); */
/* 		glColor3f(0.0, 1.0, 0.0); */
/* 		glVertex3fv(pos1); */
/* 		glVertex3fv(pos2); */

/* 		shift[0] = 0.0; shift[1] = 0.0; shift[2] = 0.3; */
/* 		cal_get_actor_bone_local_position(a, bone_id, shift, tmp); */
/* 		transform_actor_local_position_to_absolute(a, tmp, act_rot, pos2); */
/* 		glColor3f(0.0, 0.0, 1.0); */
/* 		glVertex3fv(pos1); */
/* 		glVertex3fv(pos2); */
/* 	} */

/* 	glEnd(); */
/* } */

void missiles_update(Uint32 time_diff)
{
	unsigned int i;
	float shift_t = time_diff / 1000.0;
	
	for (i = 0; i < missiles_count; ) {
		Missile *mis = &missiles_list[i];
		float dist = mis->speed * shift_t;
		mis->position[0] += mis->direction[0] * dist;
		mis->position[1] += mis->direction[1] * dist;
		mis->position[2] += mis->direction[2] * dist;
		mis->covered_distance += dist;
		mis->remaining_distance -= dist;
		if (mis->remaining_distance < -arrow_trace_length)
			missiles_remove(i);
		else
			++i;
	}
}

void missiles_draw_single(Missile *mis)
{
	switch (mis->type) {
	case MISSILE_ARROW:
		if (mis->covered_distance < arrow_trace_length) {
			glColor4f(arrow_color[0], arrow_color[1], arrow_color[2],
					  (arrow_trace_length - mis->covered_distance) / arrow_trace_length);
			glVertex3f(mis->position[0] - mis->covered_distance * mis->direction[0],
					   mis->position[1] - mis->covered_distance * mis->direction[1],
					   mis->position[2] - mis->covered_distance * mis->direction[2]);
		}
		else {
			glColor4f(arrow_color[0], arrow_color[1], arrow_color[2], 0.0);
			glVertex3f(mis->position[0] - arrow_trace_length * mis->direction[0],
					   mis->position[1] - arrow_trace_length * mis->direction[1],
					   mis->position[2] - arrow_trace_length * mis->direction[2]);
		}
		if (mis->remaining_distance < 0.0) {
			glColor4f(arrow_color[0], arrow_color[1], arrow_color[2],
					  (arrow_trace_length + mis->remaining_distance) / arrow_trace_length);
			glVertex3f(mis->position[0] + mis->remaining_distance * mis->direction[0],
					   mis->position[1] + mis->remaining_distance * mis->direction[1],
					   mis->position[2] + mis->remaining_distance * mis->direction[2]);
		}
		else {
			glColor4f(arrow_color[0], arrow_color[1], arrow_color[2], 1.0);
			glVertex3fv(mis->position);
		}
		break;
	}
}

void missiles_draw()
{
	unsigned int i;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_BLEND);
	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glLineWidth(2.0);

	glBegin(GL_LINES);
	for (i = 0; i < missiles_count; ++i)
		if (!missiles_list[i].miss_target)
			missiles_draw_single(&missiles_list[i]);
	glEnd();

	glLineStipple(1, 0x00FF);
	glEnable(GL_LINE_STIPPLE);
	glBegin(GL_LINES);
	for (i = 0; i < missiles_count; ++i)
		if (missiles_list[i].miss_target)
			missiles_draw_single(&missiles_list[i]);
	glEnd();
	glDisable(GL_LINE_STIPPLE);

/* #ifdef DEBUG */
/* 	missiles_draw_current_actor_nodes(); */
/* #endif // DEBUG */

	glPopAttrib();
}

float missiles_compute_actor_rotation(float *out_h_rot, float *out_v_rot,
									  actor *in_act, float *in_target)
{
	float cz, sz;
	float from[3], to[3], tmp[3], origin[3];
	float actor_rotation = 0;
	float act_z_rot = in_act->z_rot;

	if (in_act->rotating) {
		// the actor is already rotating so we get the final position first
		act_z_rot += in_act->rotate_z_speed * in_act->rotate_frames_left;
	}

	// we first compute the global rotation
	cz = cosf((act_z_rot) * M_PI/180.0);
	sz = sinf((act_z_rot) * M_PI/180.0);
	tmp[0] = in_target[0] - in_act->x_pos - 0.25;
	tmp[1] = in_target[1] - in_act->y_pos - 0.25;
	tmp[2] = 0.0;
	Normalize(tmp, tmp);

	actor_rotation = asinf(tmp[0] * cz - tmp[1] * sz) * 180.0/M_PI;
	if (tmp[0] * sz + tmp[1] * cz < 0.0) {
		if (actor_rotation < 0.0)
			actor_rotation = ((int)(-180-actor_rotation-22.5) / 45) * 45.0;
		else
			actor_rotation = ((int)( 180-actor_rotation+22.5) / 45) * 45.0;
	}
	else {
		if (actor_rotation < 0.0)
			actor_rotation = ((int)(actor_rotation-22.5) / 45) * 45.0;
		else
			actor_rotation = ((int)(actor_rotation+22.5) / 45) * 45.0;
	}

#ifdef DEBUG
	missiles_log_message("cos = %f ; sin = %f", cz, sz);
	missiles_log_message("direction = %f %f %f", tmp[0], tmp[1], tmp[2]);
	missiles_log_message("actor rotation = %f", actor_rotation);
#endif // DEBUG

	// we then compute the fine rotation
	cz = cosf((act_z_rot + actor_rotation) * M_PI/180.0);
	sz = sinf((act_z_rot + actor_rotation) * M_PI/180.0);

	origin[0] = in_act->x_pos + 0.25;
	origin[1] = in_act->y_pos + 0.25;
	origin[2] = get_actor_z(in_act) + 1.4 * get_actor_scale(in_act);

	missiles_log_message("compute_actor_rotation: origin=(%.2f,%.2f,%.2f), target=(%.2f,%.2f,%.2f)",
						 origin[0], origin[1], origin[2], in_target[0], in_target[1], in_target[2]);

	tmp[0] = in_target[1] - origin[1];
	tmp[1] = in_target[2] - origin[2];
	tmp[2] = in_target[0] - origin[0];
	from[0] = 0.0;
	from[1] = 0.0;
	from[2] = 1.0;

	to[0] = tmp[0] * sz - tmp[2] * cz;
	to[1] = 0.0;
	to[2] = tmp[0] * cz + tmp[2] * sz;
	Normalize(tmp, to);
	*out_h_rot = asinf(-tmp[0]);

	missiles_log_message("horizontal rotation: from=(%.2f,%.2f,%.2f), to=(%.2f,%.2f,%.2f), h_rot=%f",
						 from[0], from[1], from[2], tmp[0], tmp[1], tmp[2], *out_h_rot);

	from[0] = tmp[0];
	from[1] = tmp[1];
	from[2] = tmp[2];
	to[1] = in_target[2] - origin[2];
	Normalize(to, to);
	VCross(tmp, from, to);
	*out_v_rot = asinf(sqrt(tmp[0]*tmp[0] + tmp[1]*tmp[1] + tmp[2]*tmp[2]));
	if (to[1] < from[1]) *out_v_rot = -*out_v_rot;

	missiles_log_message("vertical rotation: from=(%.2f,%.2f,%.2f), to=(%.2f,%.2f,%.2f), v_rot=%f",
						 from[0], from[1], from[2], to[0], to[1], to[2], *out_v_rot);

	return actor_rotation;
}

unsigned int missiles_fire_arrow(actor *a, float target[3], char miss_target)
{
	unsigned int mis_id;
	float origin[3];
	float shift[3] = {0.0, get_actor_scale(a), 0.0};

	switch(a->range_weapon_type)
	{
	case RANGE_WEAPON_BOW:
		shift[1] *= arrow_length;
		break;

	case RANGE_WEAPON_CROSSBOW:
		shift[1] *= bolt_length;
		break;

	default:
		shift[1] = 0.0;
		missiles_log_message("fire_arrow: %d is an unknown range weapon type, unable to determine precisely the position of the arrow", a->range_weapon_type);
		break;
	}

	cal_get_actor_bone_absolute_position(a, 37, shift, origin);
	
	mis_id = missiles_add(MISSILE_ARROW, origin, target, arrow_speed, 0.0, miss_target);
	
	return mis_id;
}

void missiles_rotate_actor_bones(actor *a)
{
	struct CalSkeleton *skel;
	struct CalBone *bone;
	struct CalQuaternion *bone_rot, *bone_rot_abs, *hrot_quat, *vrot_quat;
	struct CalVector *vect;
	float *tmp_vect;
	float hrot, vrot, tmp;

	if (a->cal_rotation_blend < 0.0)
		return;

	skel = CalModel_GetSkeleton(a->calmodel);
	
	if (a->cal_rotation_blend < 1.0) {
		a->cal_rotation_blend += a->cal_rotation_speed;

		hrot = (a->cal_h_rot_start * (1.0 - a->cal_rotation_blend) +
				a->cal_h_rot_end * a->cal_rotation_blend);
		vrot = (a->cal_v_rot_start * (1.0 - a->cal_rotation_blend) +
				a->cal_v_rot_end * a->cal_rotation_blend);
	}
	else {
		if (fabs(a->cal_h_rot_end) < EPSILON && fabs(a->cal_v_rot_end) < EPSILON) {
			a->cal_rotation_blend = -1.0; // stop rotating bones every frames
			a->cal_h_rot_start = 0.0;
			a->cal_v_rot_start = 0.0;
#ifdef DEBUG
			missiles_log_message("stopping bones rotation");
#endif // DEBUG
		}
		else
			a->cal_rotation_blend = 1.0;

		hrot = a->cal_h_rot_end;
		vrot = a->cal_v_rot_end;

		if (a->are_bones_rotating) {
			a->are_bones_rotating = 0;
			if (a->cur_anim.anim_index >= 0 &&
				a->anim_time >= a->cur_anim.duration)
				a->busy = 0;
		}
	}

	vect = CalVector_New();
	CalVector_Set(vect, cosf(hrot), 0.0, sinf(hrot));

	hrot_quat = CalQuaternion_New();
	vrot_quat = CalQuaternion_New();

	// get the rotation of the parent bone
	bone = CalSkeleton_GetBone(skel, 0);
	bone_rot_abs = CalBone_GetRotationAbsolute(bone);

	// getting the chest bone to rotate
	bone = CalSkeleton_GetBone(skel, 11);
	bone_rot = CalBone_GetRotation(bone);

	// rotating the bone horizontally
	CalQuaternion_Set(hrot_quat, 0.0, sinf(hrot/2.0), 0.0, cosf(hrot/2.0));
	CalQuaternion_Multiply(bone_rot, hrot_quat);

	// rotating the bone vertically
	CalQuaternion_Invert(bone_rot_abs);
	CalVector_Transform(vect, bone_rot_abs);
	CalQuaternion_Invert(bone_rot_abs);
	tmp_vect = CalVector_Get(vect);
	tmp = sinf(vrot/2.0);
	CalQuaternion_Set(vrot_quat, tmp_vect[0]*tmp, tmp_vect[1]*tmp, tmp_vect[2]*tmp, cosf(vrot/2.0));
	CalQuaternion_Multiply(bone_rot, vrot_quat);

	// updating the bone state
	CalBone_CalculateState(bone);

	// rotating the cape bones
	CalQuaternion_Invert(hrot_quat);
	CalQuaternion_Invert(vrot_quat);
	vrot = -vrot;

	// rotating the bone horizontally
	if (hrot < 0.0) {
		bone = CalSkeleton_GetBone(skel, 14);
		bone_rot = CalBone_GetRotation(bone);
		CalQuaternion_Multiply(bone_rot, hrot_quat);
		CalBone_CalculateState(bone);
	}

	// rotating the bone vertically
	if (vrot < 0.0) {
		bone = CalSkeleton_GetBone(skel, 13);
		bone_rot_abs = CalBone_GetRotationAbsolute(bone);
		bone = CalSkeleton_GetBone(skel, 14);
		bone_rot = CalBone_GetRotation(bone);
	}
	else {
		bone = CalSkeleton_GetBone(skel, 14);
		bone_rot_abs = CalBone_GetRotationAbsolute(bone);
		bone = CalSkeleton_GetBone(skel, 15);
		bone_rot = CalBone_GetRotation(bone);
	}

	CalVector_Set(vect, cosf(hrot), 0.0, sinf(hrot));
	CalQuaternion_Invert(bone_rot_abs);
	CalVector_Transform(vect, bone_rot_abs);
	CalQuaternion_Invert(bone_rot_abs);
	tmp_vect = CalVector_Get(vect);
	tmp = sinf(vrot/2.0);
	CalQuaternion_Set(vrot_quat, tmp_vect[0]*tmp, tmp_vect[1]*tmp, tmp_vect[2]*tmp, cosf(vrot/2.0));
	CalQuaternion_Multiply(bone_rot, vrot_quat);
	CalBone_CalculateState(bone);

	CalVector_Delete(vect);
	CalQuaternion_Delete(hrot_quat);
	CalQuaternion_Delete(vrot_quat);
}

void missiles_aim_at_b(int actor1_id, int actor2_id)
{
	actor *act1, *act2;

	missiles_log_message("actor %d will aim at actor %d", actor1_id, actor2_id);

	act1 = get_actor_ptr_from_id(actor1_id);
	act2 = get_actor_ptr_from_id(actor2_id);

	act1->last_target_id = actor2_id;
	
	if (!act1) {
		missiles_log_message("missiles_aim_at_b: the actor %d does not exists!", actor1_id);
		return;
	}
	if (!act2) {
		missiles_log_message("missiles_aim_at_b: the actor %d does not exists!", actor2_id);
		return;
	}

	LOCK_ACTORS_LISTS();
	missiles_log_message("the target has %d bones", CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act2->calmodel)));
	if (CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act2->calmodel)) > 30)
		cal_get_actor_bone_absolute_position(act2, 13, NULL, act1->range_target);
	else
		cal_get_actor_bone_absolute_position(act2, 0, NULL, act1->range_target);
	UNLOCK_ACTORS_LISTS();

	add_command_to_actor(actor1_id, enter_aim_mode);
}

void missiles_aim_at_xyz(int actor_id, float *target)
{
	actor *act;

	missiles_log_message("actor %d will aim at target %f,%f,%f", actor_id, target[0], target[1], target[2]);

	act = get_actor_ptr_from_id(actor_id);

	if (!act) {
		missiles_log_message("missiles_aim_at_xyz: the actor %d does not exists!", actor_id);
		return;
	}

	LOCK_ACTORS_LISTS();
	memcpy(act->range_target, target, sizeof(float) * 3);
	UNLOCK_ACTORS_LISTS();

	add_command_to_actor(actor_id, enter_aim_mode);
}

void missiles_fire_a_to_b(int actor1_id, int actor2_id)
{
	actor *act1, *act2;

	missiles_log_message("actor %d will fire to actor %d", actor1_id, actor2_id);

	act1 = get_actor_ptr_from_id(actor1_id);
	act2 = get_actor_ptr_from_id(actor2_id);
	
	if (!act1) {
		missiles_log_message("missiles_fire_a_to_b: the actor %d does not exists!", actor1_id);
		return;
	}
	if (!act2) {
		missiles_log_message("missiles_fire_a_to_b: the actor %d does not exists!", actor2_id);
		return;
	}

	LOCK_ACTORS_LISTS();
	missiles_log_message("the target has %d bones", CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act2->calmodel)));
	if (CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act2->calmodel)) > 30)
		cal_get_actor_bone_absolute_position(act2, 13, NULL, act1->range_target);
	else
		cal_get_actor_bone_absolute_position(act2, 0, NULL, act1->range_target);
	UNLOCK_ACTORS_LISTS();

	add_command_to_actor(actor1_id, aim_mode_fire);
}

void missiles_fire_a_to_xyz(int actor_id, float *target)
{
	actor *act;

	missiles_log_message("actor %d will fire to target %f,%f,%f", actor_id, target[0], target[1], target[2]);

	act = get_actor_ptr_from_id(actor_id);

	if (!act) {
		missiles_log_message("missiles_fire_a_to_xyz: the actor %d does not exists!", actor_id);
		return;
	}

	LOCK_ACTORS_LISTS();
	memcpy(act->range_target, target, sizeof(float) * 3);
	UNLOCK_ACTORS_LISTS();

	add_command_to_actor(actor_id, aim_mode_fire);
}

void missiles_fire_xyz_to_b(float *origin, int actor_id)
{
	actor * act;
	unsigned int mis_id;
	float target[3];

	missiles_log_message("missile was fired from %f,%f,%f to actor %d", origin[0], origin[1], origin[2], actor_id);

	act = get_actor_ptr_from_id(actor_id);

	if (!act) {
		missiles_log_message("missiles_fire_xyz_to_b: the actor %d does not exists!", actor_id);
		return;
	}

	LOCK_ACTORS_LISTS();
	missiles_log_message("the target has %d bones", CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act->calmodel)));
	if (CalSkeleton_GetBonesNumber(CalModel_GetSkeleton(act->calmodel)) > 30)
		cal_get_actor_bone_absolute_position(act, 13, NULL, target);
	else
		cal_get_actor_bone_absolute_position(act, 0, NULL, target);
	UNLOCK_ACTORS_LISTS();

	mis_id = missiles_add(MISSILE_ARROW, origin, target, arrow_speed, 0.0, 0);
}

#endif // MISSILES