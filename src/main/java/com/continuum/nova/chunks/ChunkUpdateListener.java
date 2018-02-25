package com.continuum.nova.chunks;

import glm.vec._3.i.Vec3i;
import net.minecraft.block.state.IBlockState;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.SoundCategory;
import net.minecraft.util.SoundEvent;
import net.minecraft.util.math.BlockPos;
import net.minecraft.client.Minecraft;
import net.minecraft.world.IWorldEventListener;
import net.minecraft.world.World;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.util.PriorityQueue;

/**
 * @author ddubois
 */
public class ChunkUpdateListener implements IWorldEventListener {
    public static class BlockUpdateRange {
        public Vec3i min;
        public Vec3i max;

        BlockUpdateRange(Vec3i min, Vec3i max) {
            this.min = min;
            this.max = max;
        }

        @Override
        public String toString() {
            return "BlockUpdateRange{" +
                    "min=(" + min.x + ", " + min.y + ", " + min.z +
                    "), max=(" + max.x + ", " + max.y + ", " + max.z +
                    ")}";
        }
    }

    private static final Logger LOG = LogManager.getLogger(ChunkUpdateListener.class);

    private PriorityQueue<BlockUpdateRange> chunksToUpdate;

    public ChunkUpdateListener(PriorityQueue<BlockUpdateRange> chunksToUpdate) {
        this.chunksToUpdate = chunksToUpdate;
    }


    @Override
    public void notifyBlockUpdate(World worldIn, BlockPos pos, IBlockState oldState, IBlockState newState, int flags)
    {
        //LOG.trace("Update block at " + pos);
        //LOG.debug("Update block at " + pos);
        //LOG.info("Marking blocks in range 2 ({}, {}, {}) for render update",pos.getX(), pos.getY(), pos.getZ());

        //chunksToUpdate.add(new BlockUpdateRange(new Vec3i(pos.getX(), pos.getY(), pos.getZ()), new Vec3i(pos.getX(), pos.getY(), pos.getZ())));

        int i = pos.getX();
        int j = pos.getY();
        int k = pos.getZ();
        this.markBlockRangeForRenderUpdate(i - 1, j - 1, k - 1, i + 1, j + 1, k + 1);//, (flags & 8) != 0);
    }

    @Override
    public void notifyLightSet(BlockPos pos) {

    }

    @Override
    public void markBlockRangeForRenderUpdate(int x1, int y1, int z1, int x2, int y2, int z2) {
        LOG.info("Marking blocks in range ({}, {}, {}) to ({}, {}, {}) for render update", x1, y1, z1, x2, y2, z2);
        BlockUpdateRange range=new BlockUpdateRange(new Vec3i(x1, y1, z1), new Vec3i(x2, y2, z2));
        range.min=new Vec3i((range.min.x/16)*16,0,(range.min.z/16)*16);
        range.max=new Vec3i((range.min.x/16)*16+15,256,(range.min.z/16)*16+15);
        boolean hasIT=false;
          BlockPos blockpos = new BlockPos(Minecraft.getMinecraft().thePlayer);
          BlockPos blockpos1= new BlockPos(range.min.x,range.min.y,range.min.z);
          if(blockpos1.add(8, 8, 8).distanceSq(blockpos) > 16.0D*16.0D*4.0D){
          //  return;
          }
      /*  for( BlockUpdateRange oldRange: chunksToUpdate){
          if(oldRange.min.x==range.min.x && oldRange.min.z==range.min.z){
            LOG.error("ALREADY HAD " +oldRange.min);
          //  return;
          }
        }*/
        chunksToUpdate.add(range);
    }

    @Override
    public void playSoundToAllNearExcept(EntityPlayer player, SoundEvent soundIn, SoundCategory category, double x, double y, double z, float volume, float pitch) {

    }

    @Override
    public void playRecord(SoundEvent soundIn, BlockPos pos) {

    }

    @Override
    public void spawnParticle(int particleID, boolean ignoreRange, double xCoord, double yCoord, double zCoord, double xSpeed, double ySpeed, double zSpeed, int... parameters) {

    }

    @Override
    public void onEntityAdded(Entity entityIn) {

    }

    @Override
    public void onEntityRemoved(Entity entityIn) {

    }

    @Override
    public void broadcastSound(int soundID, BlockPos pos, int data) {

    }

    @Override
    public void playEvent(EntityPlayer player, int type, BlockPos blockPosIn, int data) {

    }

    @Override
    public void sendBlockBreakProgress(int breakerId, BlockPos pos, int progress) {
        LOG.debug("Update block at " + pos+","+progress);
    }
}
